package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.FINGERPRINT_SIZE
import com.vitorpamplona.negentropy.ID_SIZE
import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageUnit

class MessageConsumer(buffer: ByteArray? = null, lastTimestamp: Long = 0) {
    private val consumer = ByteArrayConsumer(buffer)
    private var lastTimestampIn = lastTimestamp

    fun hasItemsToConsume() = consumer.length() != 0

    fun decodeVarInt() = decodeVarInt(consumer.getBytes(::isEndVarInt))

    fun decodeTimestampIn(): Long {
        var timestamp = decodeVarInt()
        timestamp = if (timestamp == 0L) Long.MAX_VALUE else timestamp - 1
        if (lastTimestampIn == Long.MAX_VALUE || timestamp == Long.MAX_VALUE) {
            lastTimestampIn = Long.MAX_VALUE
            return Long.MAX_VALUE
        }
        timestamp += lastTimestampIn
        lastTimestampIn = timestamp
        return timestamp
    }

    fun decodeBound(): StorageUnit {
        val timestamp = decodeTimestampIn()
        val len = decodeVarInt().toInt()
        require(len <= ID_SIZE) { "bound key too long" }
        return StorageUnit(timestamp, Id(consumer.getBytes(len)))
    }

    fun decodeProtocolVersion(): Byte {
        val protocolVersion = consumer.getByte()

        check(protocolVersion in 0x60..0x6F) { throw java.lang.Error("invalid negentropy protocol version byte $protocolVersion") }

        return protocolVersion
    }

    // ---

    fun nextMode(): Mode {
        val currBound = decodeBound()
        val mode = decodeVarInt()
        return when(mode.toInt()) {
            Mode.Skip.CODE -> Mode.Skip(currBound)
            Mode.Fingerprint.CODE -> Mode.Fingerprint(currBound, consumer.getBytes(FINGERPRINT_SIZE))
            Mode.IdList.CODE -> {
                val elems = mutableListOf<Id>()
                repeat(decodeVarInt().toInt()) {
                    elems.add(Id(consumer.getBytes(ID_SIZE)))
                }
                Mode.IdList(currBound, elems)
            }
            else -> {
                throw Error("message.Mode not found")
            }
        }
    }

}

