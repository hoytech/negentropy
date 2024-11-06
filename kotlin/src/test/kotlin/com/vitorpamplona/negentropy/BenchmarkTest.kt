package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.StorageAssets.storageFrameLimitsClient
import com.vitorpamplona.negentropy.testutils.StorageAssets.storageFrameLimitsServer
import com.vitorpamplona.negentropy.testutils.benchmark
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class BenchmarkTest {
    @Test
    fun testInitialize() {
        benchmark("JIT Benchmark") { 1+1 }

        val clientDB = storageFrameLimitsClient()
        val serverDB = storageFrameLimitsServer()

        assertEquals(111, clientDB.size())
        assertEquals(147, serverDB.size())

        benchmark("Initialize Client") { Negentropy(clientDB).initiate() }
        benchmark("Initialize Server") { Negentropy(serverDB).initiate() }
    }

    @Test
    fun testReconcile() {
        benchmark("JIT Benchmark") { 1+1 }

        val clientDB = storageFrameLimitsClient()
        val serverDB = storageFrameLimitsServer()

        assertEquals(111, clientDB.size())
        assertEquals(147, serverDB.size())

        val neClient = Negentropy(clientDB)
        val neServer = Negentropy(serverDB)

        val init = neClient.initiate()

        // send to the server.
        benchmark("Reconcile") { neServer.reconcile(init) }
    }
}