package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.storage.Bound
import java.io.ByteArrayOutputStream

/**
 * Stores a skip state that can be shared between builders.
 */
class SkipDelayer(currentSkipState: Bound? = null) {
    private var delaySkipBound = currentSkipState

    fun skip(nextBound: Bound) {
        delaySkipBound = nextBound
    }

    fun addSkip(builder: MessageBuilder) {
        delaySkipBound?.let {
            builder.addSkip(Mode.Skip(it))
            delaySkipBound = null
        }
    }
}

class MessageBuilder(lastTimestamp: Long = 0L, skipStarter: SkipDelayer = SkipDelayer()) {
    private val builder = ByteArrayOutputStream(256)

    // all timestamps in a Negentropy messages are deltas from the previous one
    // the first timestamp is a unit timestamp
    private var lastTimestampOut = lastTimestamp

    // modes can be skipped in sequence. Instead of skipping them immediately
    // stores a state and only skips the last one.
    private var skipper = skipStarter

    // ---------------------
    // Branching and Merging
    // ---------------------

    // Branches a new builder while keeping the same timeout and skip status
    fun branch() = MessageBuilder(lastTimestampOut, skipper)

    // Merges a builder while keeping the same timeout and skip status
    fun merge(builder: MessageBuilder) {
        lastTimestampOut = builder.lastTimestampOut
        skipper = builder.skipper
        addByteArray(builder.toByteArray())
    }

    // -----------------
    // Utility functions
    // -----------------

    internal fun encodeTimestampOut(timestamp: Long): ByteArray {
        return if (timestamp == Long.MAX_VALUE) {
            lastTimestampOut = Long.MAX_VALUE
            encodeVarInt(0)
        } else {
            val adjustedTimestamp = timestamp - lastTimestampOut
            lastTimestampOut = timestamp
            encodeVarInt(adjustedTimestamp + 1)
        }
    }

    internal fun addByteArray(newBuffer: ByteArray) = builder.writeBytes(newBuffer)

    internal fun addNumber(n: Int) = builder.writeBytes(encodeVarInt(n))

    internal fun addNumber(n: Long) = builder.writeBytes(encodeVarInt(n))

    internal fun addTimestamp(timestamp: Long) = builder.writeBytes(encodeTimestampOut(timestamp))

    internal fun addBound(key: Bound) {
        addTimestamp(key.timestamp)
        addNumber(key.prefix.bytes.size)
        addByteArray(key.prefix.bytes)
    }

    internal fun addSkip(mode: Mode.Skip) {
        addBound(mode.nextBound)
        addNumber(Mode.Skip.CODE)
    }

    private fun addDelayedSkip() {
        skipper.addSkip(this)
    }

    // ---------------------
    // Public functions
    // ---------------------

    fun toByteArray(): ByteArray = builder.toByteArray()

    fun length() = builder.size()

    fun addProtocolVersion(version: Byte) = builder.writeBytes(byteArrayOf(version))

    fun addSkip(nextBound: Bound) = skipper.skip(nextBound)

    fun addIdList(mode: Mode.IdList) {
        addDelayedSkip()
        addBound(mode.nextBound)
        addNumber(Mode.IdList.CODE)
        addNumber(mode.ids.size)
        mode.ids.forEach { addByteArray(it.bytes) }
    }

    fun addFingerprint(mode: Mode.Fingerprint) {
        addDelayedSkip()
        addBound(mode.nextBound)
        addNumber(Mode.Fingerprint.CODE)
        addByteArray(mode.fingerprint.bytes)
    }

    fun addBounds(list: List<Mode>) {
        list.forEach {
            when (it) {
                is Mode.Fingerprint -> addFingerprint(it)
                is Mode.IdList -> addIdList(it)
                is Mode.Skip -> addSkip(it)
            }
        }
    }
}

