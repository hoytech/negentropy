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

            FingerprintCalculator().run(this, 0, 1).toHexString()
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

            FingerprintCalculator().run(this,0, 2).toHexString()
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

        assertEquals("c15ce60f185fd24ae341076caa93daa5", FingerprintCalculator().run(storage, 0, 3).toHexString())
        assertEquals("96b26d110825d4b1805fc56988a3e65d", FingerprintCalculator().run(storage, 0, 2).toHexString())
    }

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testFingerprint4() {
        val storage = StorageVector().apply {
            insert(1677981601,"224236bff9fe9c02bdeb5a13b21051696f41a18a0ee889f4b492d3341b2fe339")
            insert(1677981602,"3b99e1577caeb4d91059af0f8e64086934f9ca3eb08b61512b459075a3efd663")
            insert(1677981602,"b60a884b322338fc2b53b8ff34ed277b17397c3b3401b9e151c7e77a7c15e5fd")
            insert(1677981602,"e017a826f007171d780fda221da8ea1b9c6ec5ff69a4dc40198630e7b780a423")
            insert(1677981603,"e6753c53540b91d3483521b00da5593fc4d871b6a0dba04ab38ad900441deca0")
            insert(1677981604,"90e29d9b1c37333a25ad02a478cf356e493da355490baa860cfb16c9bd067cda")
            seal()
        }

        assertEquals("f3973242b814fe584299c94d71ae4d50", FingerprintCalculator().run(storage, 0, storage.size()).toHexString())
    }
}