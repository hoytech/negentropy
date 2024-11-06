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
        val nodes = mutableMapOf<String, Negentropy>()
        var expectedCommand: String? = null

        instructions.forEach {
            if (expectedCommand != null) {
                assertEquals(expectedCommand, it.command.source)
                expectedCommand = null
            }

            expectedCommand = ip.runLine(it, nodes)
        }
    }
}