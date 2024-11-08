package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.FINGERPRINT_SIZE
import com.vitorpamplona.negentropy.ID_SIZE
import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageUnit
import java.io.ByteArrayInputStream

class MessageConsumer(buffer: ByteArray? = null, lastTimestamp: Long = 0) {
    private val consumer = ByteArrayInputStream(buffer)

    // all timestamps in a Negentropy messages are deltas from the previous one
    // the first timestamp is a unit timestamp
    private var lastTimestampIn = lastTimestamp

    fun hasItemsToConsume() = consumer.available() > 0

    // faster to copy the function here.
    internal fun decodeVarInt(): Long {
        var res = 0L
        var currByte: Int

        do {
            currByte = consumer.read()
            res = (res shl 7) or (currByte.toLong() and 127)
        } while (!isEndVarInt(currByte))

        return res
    }

    internal fun decodeTimestampIn(): Long {
        var deltaTimestamp = decodeVarInt()
        deltaTimestamp = if (deltaTimestamp == 0L) Long.MAX_VALUE else deltaTimestamp - 1
        if (lastTimestampIn == Long.MAX_VALUE || deltaTimestamp == Long.MAX_VALUE) {
            lastTimestampIn = Long.MAX_VALUE
            return Long.MAX_VALUE
        }
        deltaTimestamp += lastTimestampIn
        lastTimestampIn = deltaTimestamp
        return deltaTimestamp
    }

    internal fun decodeBound(): StorageUnit {
        val timestamp = decodeTimestampIn()
        val len = decodeVarInt().toInt()
        require(len <= ID_SIZE) { "bound key too long" }
        return StorageUnit(timestamp, Id(consumer.readNBytes(len)))
    }

    fun decodeProtocolVersion(): Byte {
        val protocolVersion = consumer.read().toByte()

        check(protocolVersion in 0x60..0x6F) { "invalid negentropy protocol version byte $protocolVersion" }

        return protocolVersion
    }

    // ---

    fun nextMode(): Mode {
        val currBound = decodeBound()
        val mode = decodeVarInt()
        return when (mode.toInt()) {
            Mode.Skip.CODE -> Mode.Skip(currBound)
            Mode.Fingerprint.CODE -> Mode.Fingerprint(currBound, consumer.readNBytes(FINGERPRINT_SIZE))
            Mode.IdList.CODE -> Mode.IdList(currBound, List(decodeVarInt().toInt()) { Id(consumer.readNBytes(ID_SIZE)) })

            else -> {
                throw Error("message.Mode not found")
            }
        }
    }

}

