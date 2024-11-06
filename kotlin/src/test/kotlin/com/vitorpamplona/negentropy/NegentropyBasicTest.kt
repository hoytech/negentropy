package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.StorageAssets
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class NegentropyBasicTest {
    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testInitiation() {
        val expected = "6100000203eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba";
        val operator = Negentropy(StorageAssets.defaultStorage())
        val result = operator.initiate()

        assertEquals(expected, result.toHexString())
    }

    @Test
    fun testReconcile() {
        val operator = Negentropy(StorageAssets.defaultStorage())
        val init = operator.initiate()
        val result = operator.reconcile(init)

        assertEquals(null, result.msg)
        assertEquals(0, result.needIds.size)
        assertEquals(0, result.sendIds.size)
    }

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testReconcileSimple() {
        val ne = Negentropy(StorageAssets.defaultStorage())
        val result = ne.reconcile("62aabbccddeeff".hexToByteArray())

        assertEquals("61", result.msg?.toHexString())
    }
}