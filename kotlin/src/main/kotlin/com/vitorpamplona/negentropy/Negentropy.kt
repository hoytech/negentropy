package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.fingerprint.FingerprintCalculator
import com.vitorpamplona.negentropy.message.MessageBuilder
import com.vitorpamplona.negentropy.message.MessageConsumer
import com.vitorpamplona.negentropy.message.Mode
import com.vitorpamplona.negentropy.storage.Bound
import com.vitorpamplona.negentropy.storage.IStorage
import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageUnit

class Negentropy(
    internal val storage: IStorage,
    internal val frameSizeLimit: Long = 0
) {
    companion object {
        const val PROTOCOL_VERSION = 0x61.toByte() // Version 1
        const val BUCKETS_IN_MESSAGE = 16
        const val MIN_FRAME_SIZE = 4096
    }

    init {
        require(frameSizeLimit == 0L || frameSizeLimit >= MIN_FRAME_SIZE) { "frameSizeLimit too small" }
    }

    private val fingerprint = FingerprintCalculator()

    internal fun insert(timestamp: Long, id: Id) = storage.insert(timestamp, id)
    internal fun seal() = storage.seal()

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
            var upperIndex = storage.indexAtOrBeforeBound(mode.nextBound, lowerIndex, storage.size())

            when (mode) {
                is Mode.Skip -> builder.addSkip(mode.nextBound)
                is Mode.Fingerprint -> {
                    if (mode.fingerprint == fingerprint.run(storage, lowerIndex, upperIndex)) {
                        builder.addSkip(mode.nextBound)
                    } else {
                        val lineBuilder = builder.branch()

                        lineBuilder.addBounds(prepareBounds(lowerIndex, upperIndex, mode.nextBound))

                        if (!exceededFrameSizeLimit(builder.length() + lineBuilder.length())) {
                            builder.merge(lineBuilder)
                        } else {
                            // if exceeds, attaches the fingerprint and exits.
                            builder.addFingerprint(
                                Mode.Fingerprint(
                                    Bound(Long.MAX_VALUE),
                                    fingerprint.run(storage, upperIndex, storage.size())
                                )
                            )
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
                            builder.addFingerprint(
                                Mode.Fingerprint(
                                    Bound(Long.MAX_VALUE),
                                    fingerprint.run(storage, upperIndex, storage.size())
                                )
                            )
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

    private fun listIdsInRange(lowerIndex: Int, upperIndex: Int, startingNextBound: Bound, limit: Long): Mode.IdList {
        val responseIds = mutableListOf<Id>()
        var resultingNextBound = startingNextBound

        storage.iterate(lowerIndex, upperIndex) { item, index ->
            if (responseIds.size > limit) {
                resultingNextBound = Bound(item.timestamp, item.id)
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
        return prepareBounds(0, storage.size(), Bound(Long.MAX_VALUE))
    }

    private fun prepareBounds(
        lowerIndex: Int,
        upperIndex: Int,
        finalUpperBound: Bound
    ): List<Mode> {
        val numElems = upperIndex - lowerIndex

        return if (numElems < BUCKETS_IN_MESSAGE * 2) {
            listOf(Mode.IdList(finalUpperBound, storage.map(lowerIndex, upperIndex) { item -> item.id }))
        } else {
            val itemsPerBucket = numElems / BUCKETS_IN_MESSAGE
            val bucketsWithExtra = numElems % BUCKETS_IN_MESSAGE
            var currIndex = lowerIndex

            List(BUCKETS_IN_MESSAGE) {
                val bucketSize = itemsPerBucket + if (it < bucketsWithExtra) 1 else 0
                val ourFingerprint = fingerprint.run(storage, currIndex, currIndex + bucketSize)
                currIndex += bucketSize

                val nextBound = if (currIndex == upperIndex) {
                    // this is the final bucket
                    finalUpperBound
                } else {
                    // figure out where to break
                    smallestBoundPrefix(storage.getItem(currIndex - 1), storage.getItem(currIndex))
                }

                Mode.Fingerprint(nextBound, ourFingerprint)
            }
        }
    }

    private fun howManyIdsWillFit(alreadyUsed: Int): Long {
        if (frameSizeLimit == 0L) return Long.MAX_VALUE
        if (alreadyUsed > frameSizeLimit - 200L) return 0 // already passed

        return ((frameSizeLimit - 200) - alreadyUsed) / Id.SIZE
    }

    private fun exceededFrameSizeLimit(n: Int) = frameSizeLimit != 0L && n > frameSizeLimit - 200L

    private fun smallestBoundPrefix(prev: StorageUnit, curr: StorageUnit): Bound {
        return if (curr.timestamp != prev.timestamp) {
            Bound(curr.timestamp)
        } else {
            Bound(curr.timestamp, curr.id.sharedPrefixPlusMyNextChar(prev.id))
        }
    }
}