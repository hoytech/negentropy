package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.InstructionParser
import org.junit.jupiter.api.Test
import java.lang.StringBuilder
import kotlin.test.assertEquals

/**
 * Uses a custom variant of the test object to run full instructions
 */
class NegentropyJavaSriptKotlinTest {
    @Test
    fun runKotlinJs() {
        val ip = InstructionParser()

        val instructions = ip.loadFiles("/lang1-js-client.txt", "/lang1-kotlin-server.txt")
        val nodes = mutableMapOf<String, InstructionParser.Node>()
        var currentCommand: String? = null
        var currentCommandFrom: String? = null

        instructions.forEach {
            if (currentCommand != null) {
                assertEquals(it.command.source, currentCommand, "From: $currentCommandFrom to ${it.toNode}")
                currentCommand = null
            }

            currentCommand = ip.runLine(it, nodes)
            currentCommandFrom = it.toNode
        }

        val nodeClient = nodes["kotlin-side"]!!
        val nodeServer = nodes["js-side"]!!

        // test correct importation
        assertEquals(25, nodeClient.negentropy.storage.size())
        assertEquals(25, nodeServer.negentropy.storage.size())

        assertEquals(true, nodeClient.negentropy.isInitiator())
        assertEquals(false, nodeServer.negentropy.isInitiator())

        assertEquals(0, nodeServer.haves.size)
        assertEquals(0, nodeServer.needs.size)

        assertEquals(0, nodeClient.haves.size)
        assertEquals(0, nodeClient.needs.size)
    }

    @Test
    fun runJsKotlin() {
        val ip = InstructionParser()

        val instructions = ip.loadFiles("/lang2-js-server.txt", "/lang2-kotlin-client.txt")
        val nodes = mutableMapOf<String, InstructionParser.Node>()
        var currentCommand: String? = null
        var currentCommandFrom: String? = null

        instructions.forEach {
            if (currentCommand != null) {
                assertEquals(it.command.source, currentCommand, "From: $currentCommandFrom to ${it.toNode}")
                currentCommand = null
            }

            currentCommand = ip.runLine(it, nodes)
            currentCommandFrom = it.toNode
        }

        val nodeClient = nodes["js-side"]!!
        val nodeServer = nodes["kotlin-side"]!!

        // test correct importation
        assertEquals(25, nodeClient.negentropy.storage.size())
        assertEquals(25, nodeServer.negentropy.storage.size())

        assertEquals(true, nodeClient.negentropy.isInitiator())
        assertEquals(false, nodeServer.negentropy.isInitiator())

        assertEquals(0, nodeServer.haves.size)
        assertEquals(0, nodeServer.needs.size)

        assertEquals(0, nodeClient.haves.size)
        assertEquals(0, nodeClient.needs.size)
    }


    @Test
    fun runJsKotlin2() {
        val ip = InstructionParser()

        val instructions = ip.loadFiles("/lang3-js-server.txt", "/lang3-js-client.txt")
        val nodes = mutableMapOf<String, InstructionParser.Node>()
        var currentCommand: String? = null
        var currentCommandFrom: String? = null

        instructions.forEach {
            if (currentCommand != null) {
                assertEquals(it.command.source, currentCommand, "From: $currentCommandFrom to ${it.toNode}")
                currentCommand = null
            }

            currentCommand = ip.runLine(it, nodes)
            currentCommandFrom = it.toNode
        }

        val nodeClient = nodes["js-side1731072008120"]!!
        val nodeServer = nodes["js-side1731072008121"]!!

        // test correct importation
        assertEquals(24681, nodeClient.negentropy.storage.size())
        assertEquals(24715, nodeServer.negentropy.storage.size())

        assertEquals(true, nodeClient.negentropy.isInitiator())
        assertEquals(false, nodeServer.negentropy.isInitiator())

        assertEquals(0, nodeServer.haves.size)
        assertEquals(0, nodeServer.needs.size)

        assertEquals(5386, nodeClient.haves.size)
        assertEquals(5420, nodeClient.needs.size)
    }
}