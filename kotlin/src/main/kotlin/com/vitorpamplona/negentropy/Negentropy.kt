package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.fingerprint.FingerprintCalculator
import com.vitorpamplona.negentropy.message.MessageBuilder
import com.vitorpamplona.negentropy.message.MessageConsumer
import com.vitorpamplona.negentropy.message.Mode
import com.vitorpamplona.negentropy.storage.IStorage
import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageUnit

class Negentropy(
    val storage: IStorage,
    val frameSizeLimit: Long = 0
) {
    init {
        require(frameSizeLimit == 0L || frameSizeLimit >= 4096) { "frameSizeLimit too small" }
    }

    private val fingerprint = FingerprintCalculator()

    fun insert(timestamp: Long, id: Id) = storage.insert(timestamp, id)
    fun seal() = storage.seal()

    private var isInitiator: Boolean = false

    fun initiate(): ByteArray {
        check(!isInitiator) { "already initiated" }
        isInitiator = true

        val output = MessageBuilder()
        output.addProtocolVersion(PROTOCOL_VERSION)
        output.addBounds(prepareBoundsForDB(storage))
        return output.toByteArray()
    }

    fun isInitiator() = isInitiator

    @OptIn(ExperimentalStdlibApi::class)
    class ReconciliationResult(
        val msg: ByteArray?,
        val sendIds: List<Id>,
        val needIds: List<Id>
    ) {
        fun msgToString() = msg?.toHexString()
        fun sendsToString() = sendIds.joinToString(", ") { it.toHexString() }
        fun needsToString() = needIds.joinToString(", ") { it.toHexString() }
    }

    fun reconcile(query: ByteArray): ReconciliationResult {
        val haveIds = mutableSetOf<Id>()
        val needIds = mutableSetOf<Id>()

        val consumer = MessageConsumer(query)
        val protocolVersion = consumer.decodeProtocolVersion()

        val builder = MessageBuilder()
        builder.addProtocolVersion(PROTOCOL_VERSION)

        if (protocolVersion != PROTOCOL_VERSION) {
            check(!isInitiator) { "unsupported negentropy protocol version requested: ${protocolVersion - 0x60}" }
            return ReconciliationResult(builder.toByteArray(), emptyList(), emptyList())
        }

        var lowerIndex = 0

        while (consumer.hasItemsToConsume()) {
            val mode = consumer.nextMode()
            var upperIndex = storage.findNextBoundIndex(lowerIndex, storage.size(), mode.nextBound)

            when (mode) {
                is Mode.Skip -> builder.addSkip(mode.nextBound)
                is Mode.Fingerprint -> {
                    if (mode.fingerprint.contentEquals(fingerprint.run(storage, lowerIndex, upperIndex))) {
                        builder.addSkip(mode.nextBound)
                    } else {
                        val lineBuilder = builder.branch()

                        lineBuilder.addBounds(prepareBounds(lowerIndex, upperIndex, mode.nextBound, storage))

                        if (!exceededFrameSizeLimit(builder.length() + lineBuilder.length())) {
                            builder.merge(lineBuilder)
                        } else {
                            // if exceeds, attaches the fingerprint and exits.
                            builder.addFingerprint(Mode.Fingerprint(StorageUnit(Long.MAX_VALUE), fingerprint.run(storage, upperIndex, storage.size())))
                            break
                        }
                    }
                }

                is Mode.IdList -> {
                    if (isInitiator) {
                        builder.addSkip(mode.nextBound)
                        reconcileRangeIntoHavesAndNeeds(mode.ids, lowerIndex, upperIndex, haveIds, needIds)
                    } else {
                        val howManyIdsWillFit = howManyIdsWillFit(builder.length())

                        val ids = listIdsInRange(lowerIndex, upperIndex, mode.nextBound, howManyIdsWillFit)

                        upperIndex = lowerIndex + ids.ids.size

                        builder.addIdList(ids)

                        if (exceededFrameSizeLimit(builder.length())) {
                            // if exceeds, attaches the fingerprint and exits.
                            builder.addFingerprint(Mode.Fingerprint(StorageUnit(Long.MAX_VALUE), fingerprint.run(storage, upperIndex, storage.size())))
                            break
                        }
                    }
                }
            }

            lowerIndex = upperIndex
        }

        return ReconciliationResult(
            if (builder.length() == 1 && isInitiator) null else builder.toByteArray(),
            haveIds.toList(),
            needIds.toList()
        )
    }

    private fun listIdsInRange(lowerIndex: Int, upperIndex: Int, startingNextBound: StorageUnit, limit: Long): Mode.IdList {
        val responseIds = mutableListOf<Id>()
        var resultingNextBound = startingNextBound

        storage.iterate(lowerIndex, upperIndex) { item, index ->
            if (responseIds.size > limit) {
                resultingNextBound = item
                false
            } else {
                responseIds.add(item.id)
                true
            }
        }

        return Mode.IdList(resultingNextBound, responseIds)
    }

    private fun reconcileRangeIntoHavesAndNeeds(
        ids: List<Id>,
        lowerIndex: Int, upperIndex: Int,
        haves: MutableSet<Id>, needs: MutableSet<Id>
    ) {
        if (lowerIndex == upperIndex) {
            // nothing to filter in the local db
            needs.addAll(ids)
        } else {
            val theirElems = ids.toMutableSet()
            storage.forEach(lowerIndex, upperIndex) { item ->
                if (!theirElems.contains(item.id)) {
                    haves.add(item.id)
                } else {
                    theirElems.remove(item.id)
                }
            }
            needs.addAll(theirElems)
        }
    }

    private fun prepareBoundsForDB(storage: IStorage): List<Mode> {
        return prepareBounds(
            lowerIndex = 0,
            upperIndex = storage.size(),
            startingUpperBound = StorageUnit(Long.MAX_VALUE),
            storage = storage
        )
    }

    private fun prepareBounds(
        lowerIndex: Int,
        upperIndex: Int,
        startingUpperBound: StorageUnit,
        storage: IStorage
    ): List<Mode> {
        val numElems = upperIndex - lowerIndex
        val buckets = BUCKETS_IN_MESSAGE

        val result = mutableListOf<Mode>()

        if (numElems < buckets * 2) {
            result.add(Mode.IdList(startingUpperBound, storage.map(lowerIndex, upperIndex) { item -> item.id }))
        } else {
            val itemsPerBucket = numElems / buckets
            val bucketsWithExtra = numElems % buckets
            var currIndex = lowerIndex

            repeat(buckets) { i ->
                val bucketSize = itemsPerBucket + if (i < bucketsWithExtra) 1 else 0
                val ourFingerprint = fingerprint.run(storage, currIndex, currIndex + bucketSize)
                currIndex += bucketSize

                val nextBound = if (currIndex == upperIndex) {
                    // this is the final bucket
                    startingUpperBound
                } else {
                    // figure out where to break
                    smallestBound(storage.getItem(currIndex - 1), storage.getItem(currIndex))
                }

                result.add(Mode.Fingerprint(nextBound, ourFingerprint))
            }
        }

        return result
    }

    private fun howManyIdsWillFit(alreadyUsed: Int): Long {
        if (frameSizeLimit == 0L) return Long.MAX_VALUE
        if (alreadyUsed > frameSizeLimit - 200L) return 0 // already passed

        return ((frameSizeLimit - 200) - alreadyUsed) / ID_SIZE
    }

    private fun exceededFrameSizeLimit(n: Int) = frameSizeLimit != 0L && n > frameSizeLimit - 200L

    private fun smallestBound(prev: StorageUnit, curr: StorageUnit): StorageUnit {
        return if (curr.timestamp != prev.timestamp) {
            StorageUnit(curr.timestamp)
        } else {
            StorageUnit(curr.timestamp, curr.id.sharedPrefixPlusMyNextChar(prev.id))
        }
    }
}