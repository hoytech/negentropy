package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.fingerprint.FingerprintCalculator
import com.vitorpamplona.negentropy.message.MessageBuilder
import com.vitorpamplona.negentropy.message.MessageConsumer
import com.vitorpamplona.negentropy.message.Mode
import com.vitorpamplona.negentropy.storage.IStorage
import com.vitorpamplona.negentropy.storage.StorageUnit

class Negentropy(
    private val storage: IStorage,
    private val frameSizeLimit: Int = 0
) {
    init {
        require(frameSizeLimit == 0 || frameSizeLimit >= 4096) { "frameSizeLimit too small" }
    }

    private var isInitiator: Boolean = false

    fun initiate(): ByteArray {
        check(!isInitiator) { "already initiated" }
        isInitiator = true

        val output = MessageBuilder()
        output.addProtocolVersion(PROTOCOL_VERSION)
        output.addBounds(prepareBoundsForDB(storage))
        return output.unwrap()
    }

    fun setInitiator() {
        isInitiator = true
    }

    @OptIn(ExperimentalStdlibApi::class)
    class ReconciliationResult(
        val msg: ByteArray?,
        val sendIds: List<ByteArray>,
        val needIds: List<ByteArray>
    ) {
        fun msgToString() = msg?.joinToString(", ") { it.toHexString() }
        fun sendsToString() = sendIds.joinToString(", ") { it.toHexString() }
        fun needsToString() = needIds.joinToString(", ") { it.toHexString() }
    }

    @OptIn(ExperimentalStdlibApi::class)
    fun reconcile(query: ByteArray): ReconciliationResult {
        val haveIds = mutableSetOf<String>()
        val needIds = mutableSetOf<String>()
        val consumer = MessageConsumer(query)

        val fullOutput = MessageBuilder()
        fullOutput.addProtocolVersion(PROTOCOL_VERSION)

        val protocolVersion = consumer.decodeProtocolVersion()

        if (protocolVersion != PROTOCOL_VERSION) {
            check(!isInitiator) { "unsupported negentropy protocol version requested: ${protocolVersion - 0x60}" }
            return ReconciliationResult(fullOutput.unwrap(), emptyList(), emptyList())
        }

        val storageSize = storage.size()
        var prevBound = StorageUnit(0, ByteArray(0))
        var prevIndex = 0
        var skip = false

        while (consumer.hasItemsToConsume()) {
            val o = MessageBuilder()
            val mode = consumer.nextMode()

            val lower = prevIndex
            val upper = storage.findLowerBound(prevIndex, storageSize, mode.nextBound)

            when (mode) {
                is Mode.Skip -> skip = true
                is Mode.Fingerprint -> {
                    val ourFingerprint = FingerprintCalculator.fingerprint(storage, lower, upper)
                    if (mode.fingerprint.contentEquals(ourFingerprint)) {
                        skip = true
                    } else {
                        if (skip) {
                            skip = false
                            o.addSkip(prevBound)
                        }
                        val list = prepareBounds(lower, upper, mode.nextBound, storage)
                        o.addBounds(list)
                    }
                }

                is Mode.IdList -> {
                    if (isInitiator) {
                        val theirElems = mode.ids.mapTo(HashSet()) {
                            it.toHexString()
                        }

                        skip = true

                        storage.forEach(lower, upper) { item ->
                            val k = item.id.toHexString()
                            if (!theirElems.contains(k)) {
                                haveIds.add(k)
                            } else {
                                theirElems.remove(k)
                            }
                        }

                        theirElems.forEach { k -> needIds.add(k) }
                    } else {
                        if (skip) {
                            skip = false
                            o.addSkip(prevBound)
                        }

                        val responseIds = mutableListOf<ByteArray>()
                        var endBound = mode.nextBound

                        storage.iterate(lower, upper) { item ->
                            if (exceededFrameSizeLimit(fullOutput.length() + (responseIds.size * ID_SIZE))) {
                                endBound = item
                                return@iterate false
                            }
                            responseIds.add(item.id)
                            true
                        }

                        o.addIdList(endBound, responseIds)

                        fullOutput.addByteArray(o.unwrap())
                    }
                }
            }

            if (exceededFrameSizeLimit(fullOutput.length() + o.length())) {
                val remainingFingerprint = FingerprintCalculator.fingerprint(storage, upper, storageSize)
                fullOutput.addFingerprint(remainingFingerprint)
                break
            } else {
                fullOutput.addByteArray(o.unwrap())
            }

            prevIndex = upper
            prevBound = mode.nextBound
        }

        return ReconciliationResult(
            if (fullOutput.length() == 1 && isInitiator) null else fullOutput.unwrap(),
            haveIds.map { it.hexToByteArray() },
            needIds.map { it.hexToByteArray() }
        )
    }

    private fun prepareBoundsForDB(storage: IStorage): List<Mode> {
        return prepareBounds(
            lower = 0,
            upper = storage.size(),
            upperBound = StorageUnit(
                Long.MAX_VALUE,
                ByteArray(0)
            ),
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
                val ourFingerprint = FingerprintCalculator.fingerprint(storage, curr, curr + bucketSize)
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
        return frameSizeLimit != 0 && n > frameSizeLimit - 200
    }

    private fun getMinimalBound(prev: StorageUnit, curr: StorageUnit): StorageUnit {
        return if (curr.timestamp != prev.timestamp) {
            StorageUnit(curr.timestamp, ByteArray(0))
        } else {
            var sharedPrefixBytes = 0
            val currKey = curr.id
            val prevKey = prev.id

            for (i in 0 until ID_SIZE) {
                if (currKey[i] != prevKey[i]) break
                sharedPrefixBytes++
            }

            StorageUnit(curr.timestamp, currKey.copyOfRange(0, sharedPrefixBytes + 1))
        }
    }
}