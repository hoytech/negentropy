package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.StorageAssets.storageClientWLimits
import com.vitorpamplona.negentropy.testutils.StorageAssets.storageServerWLimits
import com.vitorpamplona.negentropy.testutils.benchmark
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class BenchmarkTest {
    @Test
    fun testInitialize() {
        val clientDB = storageClientWLimits()
        val serverDB = storageServerWLimits()

        assertEquals(111, clientDB.size())
        assertEquals(147, serverDB.size())

        benchmark("Initialize Client") { Negentropy(clientDB).initiate() }
        benchmark("Initialize Server") { Negentropy(serverDB).initiate() }
    }

    @Test
    fun testReconcile() {
        val clientDB = storageClientWLimits()
        val serverDB = storageServerWLimits()

        assertEquals(111, clientDB.size())
        assertEquals(147, serverDB.size())

        val neClient = Negentropy(clientDB)
        val neServer = Negentropy(serverDB)

        val init = neClient.initiate()

        // send to the server.
        benchmark("Reconcile") { neServer.reconcile(init) }
    }
}