package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.InstructionParser
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

/**
 * Uses a custom variant of the test object to run full instructions
 */
class NegentropyMultipleRoundsTest {
    @Test
    fun runFullInstructions() {
        val ip = InstructionParser()

        val instructions = ip.loadFiles("/multiplerounds-node1.txt", "/multiplerounds-node2.txt")
        val nodes = mutableMapOf<String, InstructionParser.Node>()
        var producedCommand: String? = null

        instructions.forEach {
            if (producedCommand != null) {
                assertEquals(it.command.source, producedCommand)
                producedCommand = null
            }

            producedCommand = ip.runLine(it, nodes)
        }

        val nodeClient = nodes["559789284344708"]!!
        val nodeServer = nodes["559789284489666"]!!

        // test correct importation
        assertEquals(0, nodeClient.negentropy.storage.size())
        assertEquals(100000, nodeServer.negentropy.storage.size())

        assertEquals(true, nodeClient.negentropy.isInitiator())
        assertEquals(false, nodeServer.negentropy.isInitiator())

        assertEquals(0, nodeServer.haves.size)
        assertEquals(0, nodeServer.needs.size)

        assertEquals(0, nodeClient.haves.size)
        assertEquals(100000, nodeClient.needs.size)
    }
}