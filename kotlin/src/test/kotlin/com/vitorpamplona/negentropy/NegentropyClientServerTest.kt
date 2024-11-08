package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.StorageAssets
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class NegentropyClientServerTest {
    @Test
    fun testReconcileDifferentDBs() {
        val client = Negentropy(StorageAssets.storageP1())
        val server = Negentropy(StorageAssets.storageP2())

        val init = client.initiate()
        val resultFor1 = server.reconcile(init)

        assertEquals("610000020239b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba", resultFor1.msgToString())
        assertEquals("", resultFor1.needsToString())
        assertEquals("", resultFor1.sendsToString())

        val resultFor2 = client.reconcile(resultFor1.msg!!)

        assertEquals(null, resultFor2.msgToString())
        assertEquals("39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0", resultFor2.needsToString())
        assertEquals("eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c", resultFor2.sendsToString())
    }

    @Test
    fun testReconcileEqualDB() {
        val client = Negentropy(StorageAssets.storageP1())
        val server = Negentropy(StorageAssets.storageP1())

        val init = client.initiate()

        val resultFor1 = server.reconcile(init)

        assertEquals("6100000202eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008cabc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba", resultFor1.msgToString())
        assertEquals("", resultFor1.needsToString())
        assertEquals("", resultFor1.sendsToString())

        val resultFor2 = client.reconcile(resultFor1.msg!!)

        assertEquals(null, resultFor2.msg)
        assertEquals("", resultFor2.needsToString())
        assertEquals("", resultFor2.sendsToString())
    }
}