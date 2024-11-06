package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageUnit
import java.io.ByteArrayOutputStream

class MessageBuilder(lastTimestamp: Long = 0L) {
    private val builder = ByteArrayOutputStream(256)
    private var lastTimestampOut = lastTimestamp

    fun branch() = MessageBuilder(lastTimestampOut)

    fun merge(builder: MessageBuilder) {
        lastTimestampOut = builder.lastTimestampOut
        addByteArray(builder.toByteArray())
    }

    fun toByteArray() = builder.toByteArray()

    fun length() = builder.size()

    fun encodeTimestampOut(timestamp: Long): ByteArray {
        return if (timestamp == Long.MAX_VALUE) {
            lastTimestampOut = Long.MAX_VALUE
            encodeVarInt(0)
        } else {
            val adjustedTimestamp = timestamp - lastTimestampOut
            lastTimestampOut = timestamp
            encodeVarInt(adjustedTimestamp + 1)
        }
    }

    fun addByteArray(newBuffer: ByteArray) = builder.writeBytes(newBuffer)

    fun addNumber(n: Int) = builder.writeBytes(encodeVarInt(n))

    fun addNumber(n: Long) = builder.writeBytes(encodeVarInt(n))

    fun addTimestamp(timestamp: Long) = builder.writeBytes(encodeTimestampOut(timestamp))

    fun addBound(key: StorageUnit) {
        addTimestamp(key.timestamp)
        addNumber(key.id.bytes.size)
        addByteArray(key.id.bytes)
    }

    fun addProtocolVersion(version: Byte) = builder.writeBytes(byteArrayOf(version))

    // ------

    fun addSkip(prevBound: StorageUnit) {
        addBound(prevBound)
        addNumber(Mode.Skip.CODE)
    }

    fun addSkip(mode: Mode.Skip) = addSkip(mode.nextBound)

    fun addIdList(nextBound: StorageUnit, ids: List<Id>) {
        addBound(nextBound)
        addNumber(Mode.IdList.CODE)
        addNumber(ids.size)
        ids.forEach {
            addByteArray(it.bytes)
        }
    }

    fun addIdList(mode: Mode.IdList) = addIdList(mode.nextBound, mode.ids)

    fun addFingerprint(fingerprint: ByteArray) = addFingerprint(StorageUnit(Long.MAX_VALUE), fingerprint)

    fun addFingerprint(nextBound: StorageUnit, fingerprint: ByteArray) {
        addBound(nextBound)
        addNumber(Mode.Fingerprint.CODE)
        addByteArray(fingerprint)
    }

    fun addFingerprint(mode: Mode.Fingerprint) = addFingerprint(mode.nextBound, mode.fingerprint)

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

