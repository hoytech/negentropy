package com.vitorpamplona.negentropy.fingerprint

import com.vitorpamplona.negentropy.ID_SIZE
import java.nio.ByteBuffer
import java.nio.ByteOrder

class FingerprintAccumulator {
    private var buf: ByteArray = ByteArray(ID_SIZE)

    fun bytes() = buf

    fun add(otherBuf: ByteArray) {
        var currCarry = 0L
        val p = ByteBuffer.wrap(buf).order(ByteOrder.LITTLE_ENDIAN)
        val po = ByteBuffer.wrap(otherBuf).order(ByteOrder.LITTLE_ENDIAN)

        for (i in 0 until 8) {
            val offset = i * 4

            // must get 4 bytes from butter, convert signed to unsigned int and
            // place it in a bigger variable to allow the if below
            val pValue = p.getInt(offset).toUInt().toLong()
            val poValue = po.getInt(offset).toUInt().toLong()

            val next = pValue + currCarry + poValue

            p.putInt(offset, (next and 0xFFFFFFFF).toInt())
            currCarry = if (next > 0xFFFFFFFF) 1 else 0
        }
    }

    fun negate() {
        val p = ByteBuffer.wrap(buf)

        for (i in 0 until 8) {
            val offset = i * 4
            p.putInt(offset, p.getInt(offset).inv())
        }

        val one = ByteArray(ID_SIZE)
        one[0] = 1
        add(one)
    }
}

