package com.vitorpamplona.negentropy.fingerprint

import com.vitorpamplona.negentropy.storage.StorageVector
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class FingerprintTest {

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testFingerprint() {
        val result = with(StorageVector()) {
            insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c")
            seal()

            FingerprintCalculator.fingerprint(this, 0, 1).toHexString()
        }

        assertEquals("6d74e1212fcafc8a81d090f0ffbde8b0", result)
    }

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testFingerprint2() {
        val result = with(StorageVector()) {
            insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c")
            insert(1678011278, "39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0")

            seal()

            FingerprintCalculator.fingerprint(this,0, 2).toHexString()
        }

        assertEquals("96b26d110825d4b1805fc56988a3e65d", result)
    }

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testFingerprint3() {
        val storage = StorageVector().apply {
            insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c")
            insert(1678011278, "39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0")
            insert(1678011279, "abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba")

            seal()
        }

        assertEquals("c15ce60f185fd24ae341076caa93daa5", FingerprintCalculator.fingerprint(storage, 0, 3).toHexString())
        assertEquals("96b26d110825d4b1805fc56988a3e65d", FingerprintCalculator.fingerprint(storage, 0, 2).toHexString())
    }
}