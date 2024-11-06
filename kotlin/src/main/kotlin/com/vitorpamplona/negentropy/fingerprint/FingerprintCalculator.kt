package com.vitorpamplona.negentropy.fingerprint

import com.vitorpamplona.negentropy.FINGERPRINT_SIZE
import com.vitorpamplona.negentropy.ID_SIZE
import com.vitorpamplona.negentropy.message.encodeVarInt
import com.vitorpamplona.negentropy.storage.IStorage
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest

class FingerprintCalculator {
    private var buf: ByteArray = ByteArray(ID_SIZE)

    private fun add(otherBuf: ByteArray) {
        var currCarry = 0L
        val p = ByteBuffer.wrap(buf).order(ByteOrder.LITTLE_ENDIAN)
        val po = ByteBuffer.wrap(otherBuf).order(ByteOrder.LITTLE_ENDIAN)

        for (i in 0 until 8) {
            val offset = i * 4

            // must get 4 bytes from butter, convert signed to unsigned int and
            // place it in a bigger variable to allow the if below
            val next = p.getInt(offset).toUInt().toLong() + currCarry + po.getInt(offset).toUInt().toLong()

            p.putInt(offset, (next and 0xFFFFFFFF).toInt())
            currCarry = if (next > 0xFFFFFFFF) 1 else 0
        }
    }

    private fun negate() {
        val p = ByteBuffer.wrap(buf)

        for (i in 0 until 8) {
            val offset = i * 4
            p.putInt(offset, p.getInt(offset).inv())
        }

        val one = ByteArray(ID_SIZE)
        one[0] = 1
        add(one)
    }

    fun fingerprint(storage: IStorage, begin: Int, end: Int): ByteArray {
        storage.forEach(begin, end) { item ->
            add(item.id.bytes)
        }

        return fingerprint(buf + encodeVarInt(end - begin))
    }

    private fun fingerprint(bytes: ByteArray) = sha256(bytes).copyOfRange(0, FINGERPRINT_SIZE)

    private fun sha256(slice: ByteArray) = MessageDigest.getInstance("SHA-256").digest(slice)
}