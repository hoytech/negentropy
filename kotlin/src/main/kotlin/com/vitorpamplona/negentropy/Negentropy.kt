package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.fingerprint.FingerprintCalculator
import com.vitorpamplona.negentropy.message.MessageBuilder
import com.vitorpamplona.negentropy.message.MessageConsumer
import com.vitorpamplona.negentropy.message.Mode
import com.vitorpamplona.negentropy.storage.IStorage
import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageUnit

class Negentropy(
    private val storage: IStorage,
    val frameSizeLimit: Long = 0
) {
    init {
        require(frameSizeLimit == 0L || frameSizeLimit >= 4096) { "frameSizeLimit too small" }
    }

    val fingerprint = FingerprintCalculator()

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

    fun setInitiator() {
        isInitiator = true
    }

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

        val builder = MessageBuilder()
        builder.addProtocolVersion(PROTOCOL_VERSION)

        val protocolVersion = consumer.decodeProtocolVersion()

        if (protocolVersion != PROTOCOL_VERSION) {
            check(!isInitiator) { "unsupported negentropy protocol version requested: ${protocolVersion - 0x60}" }
            return ReconciliationResult(builder.toByteArray(), emptyList(), emptyList())
        }

        val storageSize = storage.size()
        var prevBound = StorageUnit(0)
        var prevIndex = 0
        var skip = false

        while (consumer.hasItemsToConsume()) {
            var lineBuilder = builder.branch()
            val mode = consumer.nextMode()

            val lowerIndex = prevIndex
            var upperIndex = storage.findLowerBound(prevIndex, storageSize, mode.nextBound)

            when (mode) {
                is Mode.Skip -> skip = true
                is Mode.Fingerprint -> {
                    if (mode.fingerprint.contentEquals(fingerprint.run(storage, lowerIndex, upperIndex))) {
                        skip = true
                    } else {
                        if (skip) {
                            skip = false
                            lineBuilder.addSkip(prevBound)
                        }
                        lineBuilder.addBounds(prepareBounds(lowerIndex, upperIndex, mode.nextBound, storage))
                    }
                }

                is Mode.IdList -> {
                    if (isInitiator) {
                        val theirElems = mode.ids.toMutableSet()

                        skip = true

                        storage.forEach(lowerIndex, upperIndex) { item ->
                            if (!theirElems.contains(item.id)) {
                                haveIds.add(item.id)
                            } else {
                                theirElems.remove(item.id)
                            }
                        }

                        theirElems.forEach { k -> needIds.add(k) }
                    } else {
                        if (skip) {
                            skip = false
                            lineBuilder.addSkip(prevBound)
                        }

                        val responseIds = mutableListOf<Id>()
                        var endBound = mode.nextBound

                        storage.iterate(lowerIndex, upperIndex) { item, index ->
                            if (exceededFrameSizeLimit(builder.length() + (responseIds.size * ID_SIZE))) {
                                endBound = item
                                upperIndex = index
                                return@iterate false
                            }
                            responseIds.add(item.id)
                            true
                        }

                        lineBuilder.addIdList(endBound, responseIds)

                        builder.merge(lineBuilder)
                        lineBuilder = builder.branch()
                    }
                }
            }

            if (exceededFrameSizeLimit(builder.length() + lineBuilder.length())) {
                // if exceeds, attaches the fingerprint and exits.
                builder.addFingerprint(fingerprint.run(storage, upperIndex, storageSize))
                break
            } else {
                builder.merge(lineBuilder)
            }

            prevIndex = upperIndex
            prevBound = mode.nextBound
        }

        return ReconciliationResult(
            if (builder.length() == 1 && isInitiator) null else builder.toByteArray(),
            haveIds.toList(),
            needIds.toList()
        )
    }

    private fun prepareBoundsForDB(storage: IStorage): List<Mode> {
        return prepareBounds(
            lower = 0,
            upper = storage.size(),
            upperBound = StorageUnit(Long.MAX_VALUE),
            storage = storage
        )
    }

    private fun prepareBounds(lower: Int, upper: Int, upperBound: StorageUnit, storage: IStorage): List<Mode> {
        val numElems = upper - lower
        val buckets = 16

        val result = mutableListOf<Mode>()

        if (numElems < buckets * 2) {
            result.add(Mode.IdList(upperBound, storage.map(lower, upper) { item -> item.id }))
        } else {
            val itemsPerBucket = numElems / buckets
            val bucketsWithExtra = numElems % buckets
            var curr = lower

            repeat(buckets) { i ->
                val bucketSize = itemsPerBucket + if (i < bucketsWithExtra) 1 else 0
                val ourFingerprint = fingerprint.run(storage, curr, curr + bucketSize)
                curr += bucketSize

                val nextBound = if (curr == upper) upperBound else {
                    val prevItem = storage.getItem(curr - 1)
                    val currItem = storage.getItem(curr)
                    getMinimalBound(prevItem, currItem)
                }

                result.add(Mode.Fingerprint(nextBound, ourFingerprint))
            }
        }

        return result
    }

    private fun exceededFrameSizeLimit(n: Int): Boolean {
        return frameSizeLimit != 0L && n > frameSizeLimit - 200L
    }

    private fun getMinimalBound(prev: StorageUnit, curr: StorageUnit): StorageUnit {
        return if (curr.timestamp != prev.timestamp) {
            StorageUnit(curr.timestamp)
        } else {
            StorageUnit(curr.timestamp, curr.id.sharedPrefix(prev.id))
        }
    }
}