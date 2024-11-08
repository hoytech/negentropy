package com.vitorpamplona.negentropy.fingerprint

import com.vitorpamplona.negentropy.FINGERPRINT_SIZE
import com.vitorpamplona.negentropy.ID_SIZE
import com.vitorpamplona.negentropy.message.encodeVarInt
import com.vitorpamplona.negentropy.storage.IStorage
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import java.util.*

class FingerprintCalculator {
    companion object {
        val ZERO_RANGE_FINGERPRINT = FingerprintCalculator().fingerprint(ByteArray(ID_SIZE) + encodeVarInt(0))
    }

    val buf: ByteArray = ByteArray(ID_SIZE)

    private fun add(base: ByteArray, toAdd: ByteArray) {
        var currCarry = 0L
        val p = ByteBuffer.wrap(base).order(ByteOrder.LITTLE_ENDIAN)
        val po = ByteBuffer.wrap(toAdd).order(ByteOrder.LITTLE_ENDIAN)

        for (i in 0 until 8) {
            val offset = i * 4

            // must get 4 bytes from butter, convert signed to unsigned int and
            // place it in a bigger variable to allow the if below
            val next = p.getInt(offset).toUInt().toLong() + currCarry + po.getInt(offset).toUInt().toLong()

            p.putInt(offset, (next and 0xFFFFFFFF).toInt())
            currCarry = if (next > 0xFFFFFFFF) 1 else 0
        }
    }

    fun run(storage: IStorage, begin: Int, end: Int): ByteArray {
        if (begin == end) return ZERO_RANGE_FINGERPRINT

        Arrays.fill(buf, 0)

        storage.forEach(begin, end) { item ->
            add(buf, item.id.bytes)
        }

        return fingerprint(buf + encodeVarInt(end - begin))
    }

    private fun fingerprint(bytes: ByteArray) = sha256(bytes).copyOfRange(0, FINGERPRINT_SIZE)

    private fun sha256(slice: ByteArray) = MessageDigest.getInstance("SHA-256").digest(slice)
}