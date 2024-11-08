package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.InstructionParser
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

/**
 * Uses a custom variant of the test object to run full instructions
 */
class NegentropyMultipleRoundsDataBothSidesTest {
    @Test
    fun runFullInstructions() {
        val ip = InstructionParser()

        val instructions = ip.loadFiles("/multiplerounds-databothsides-node1.txt", "/multiplerounds-databothsides-node2.txt")
        val nodes = mutableMapOf<String, InstructionParser.Node>()
        var producedCommand: String? = null

        instructions.forEach {
            if (producedCommand != null) {
                assertEquals(it.command.source, producedCommand)
                producedCommand = null
            }

            producedCommand = ip.runLine(it, nodes)
        }

        val nodeClient = nodes["641313757126791"]!!
        val nodeServer = nodes["641313757240583"]!!

        // test correct importation
        assertEquals(215908, nodeClient.negentropy.storage.size())
        assertEquals(215613, nodeServer.negentropy.storage.size())

        assertEquals(true, nodeClient.negentropy.isInitiator())
        assertEquals(false, nodeServer.negentropy.isInitiator())

        val clientDB = nodeClient.negentropy.storage.map { it.id }.toSet()
        val serverDB = nodeServer.negentropy.storage.map { it.id }.toSet()

        val haveOnBoth = clientDB.intersect(serverDB)
        val onlyOnClient = clientDB - serverDB
        val onlyOnServer = serverDB - clientDB

        val clientRealNeeds = onlyOnServer
        val serverRealNeeds = onlyOnClient

        assertEquals(213523, haveOnBoth.size)
        assertEquals(2385, onlyOnClient.size)
        assertEquals(2090, onlyOnServer.size)

        assertEquals(0, nodeServer.haves.size)
        assertEquals(0, nodeServer.needs.size)

        val uniqueHaves = nodeClient.haves.toSet()

        assertEquals(clientRealNeeds.size, nodeClient.needs.size)
        assertEquals(serverRealNeeds.size, uniqueHaves.size)

        val haveDuplicationOverhead = nodeClient.haves.groupBy { it }.filter { it.value.size > 1 }

        assertEquals(269, haveDuplicationOverhead.size)
    }
}