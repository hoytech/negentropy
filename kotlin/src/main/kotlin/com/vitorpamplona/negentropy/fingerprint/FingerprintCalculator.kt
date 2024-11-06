package com.vitorpamplona.negentropy.fingerprint

import com.vitorpamplona.negentropy.FINGERPRINT_SIZE
import com.vitorpamplona.negentropy.message.encodeVarInt
import com.vitorpamplona.negentropy.storage.IStorage
import java.security.MessageDigest

object FingerprintCalculator {
    fun fingerprint(storage: IStorage, begin: Int, end: Int): ByteArray {
        val out = FingerprintAccumulator()

        storage.forEach(begin, end) { item ->
            out.add(item.id.bytes)
        }

        return fingerprint(out.bytes() + encodeVarInt(end - begin))
    }

    private fun fingerprint(bytes: ByteArray) = sha256(bytes).copyOfRange(0, FINGERPRINT_SIZE)

    private fun sha256(slice: ByteArray) = MessageDigest.getInstance("SHA-256").digest(slice)
}