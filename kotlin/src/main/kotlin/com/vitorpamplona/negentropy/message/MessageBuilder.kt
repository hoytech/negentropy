package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.storage.StorageUnit

class MessageBuilder(lastTimestamp: Long = 0L) {
    private val builder = ByteArrayBuilder()
    private var lastTimestampOut = lastTimestamp

    fun branch() = MessageBuilder(lastTimestampOut)

    fun merge(builder: MessageBuilder) {
        lastTimestampOut = builder.lastTimestampOut
        addByteArray(builder.unwrap())
    }

    fun unwrap() = builder.unwrap()

    fun length() = builder.length()

    fun addByteArray(newBuffer: ByteArray) = builder.extend(newBuffer)

    fun addNumber(n: Int) = builder.extend(encodeVarInt(n))

    fun addNumber(n: Long) = builder.extend(encodeVarInt(n))

    fun encodeTimestampOut(timestamp: Long): ByteArray {
        if (timestamp == Long.MAX_VALUE) {
            lastTimestampOut = Long.MAX_VALUE
            return encodeVarInt(0)
        }

        val temp = timestamp
        val adjustedTimestamp = timestamp - lastTimestampOut
        lastTimestampOut = temp
        return encodeVarInt(adjustedTimestamp + 1)
    }

    fun addTimestamp(timestamp: Long) = builder.extend(encodeTimestampOut(timestamp))

    fun addBound(key: StorageUnit) {
        addTimestamp(key.timestamp)
        addNumber(key.id.size)
        addByteArray(key.id)
    }

    fun addProtocolVersion(version: Byte) = builder.extend(byteArrayOf(version))

    // ------

    fun addSkip(prevBound: StorageUnit) {
        addBound(prevBound)
        addNumber(Mode.Skip.CODE)
    }

    fun addSkip(mode: Mode.Skip) = addSkip(mode.nextBound)

    fun addIdList(nextBound: StorageUnit, ids: List<ByteArray>) {
        addBound(nextBound)
        addNumber(Mode.IdList.CODE)
        addNumber(ids.size)
        ids.forEach {
            addByteArray(it)
        }
    }

    fun addIdList(mode: Mode.IdList) = addIdList(mode.nextBound, mode.ids)

    fun addFingerprint(fingerprint: ByteArray) = addFingerprint(StorageUnit(Long.MAX_VALUE, ByteArray(0)), fingerprint)

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

