package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageVector
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

/**
 * Uses a custom variant of the test object to run full instructions
 */
class NegentropyMultipleRoundsTest {
    class Instruction(
        val toNode: String,
        val time: Long,
        val command: String,
    )

    private fun loadFile(file: String): List<Instruction> {
        val input = object {}.javaClass.getResourceAsStream(file) ?: error("Cannot open/find the file $file")
        val list = mutableListOf<Instruction>()

        input.bufferedReader().lines().forEach {
            if (it.contains(",")) {
                val (time, toNode, command) = it.split(",", limit = 3)
                list.add(Instruction(toNode, time.toLong(), command))
            }
        }

        return list
    }

    private fun loadFiles(file1: String, file2: String): List<Instruction> {
        return (loadFile(file1) + loadFile(file2)).sortedBy { it.time }
    }

    @OptIn(ExperimentalStdlibApi::class)
    fun parseLine(line: Instruction, nodes: MutableMap<String, Negentropy>): String? {
        val items = line.command.split(",")
        val ne = nodes[line.toNode]

        when (items[0]) {
            "create" -> {
                if (items.size != 2) throw Error("too few items")
                val frameSizeLimit = items[1].toLongOrNull() ?: throw Error("Invalid framesoze format")
                if (nodes.contains(line.toNode)) { throw Error("Node already exist") }

                nodes[line.toNode] = Negentropy(StorageVector(), frameSizeLimit)
            }

            "item" -> {
                if (ne == null) throw Error("Negentropy not created for this Node ${line.toNode}")
                if (items.size != 3) throw Error("too few items")
                val created = items[1].toLongOrNull() ?: throw Error("Invalid timestamp format")
                val id = items[2].trim()
                ne.insert(created, Id(id.hexToByteArray()))
            }

            "seal" -> {
                if (ne == null) throw Error("Negentropy not created for this Node")
                ne.seal()
            }

            "initiate" -> {
                if (ne == null) throw Error("Negentropy not created for this Node")
                val q = ne.initiate()
                if (ne.frameSizeLimit > 0 && q.size > ne.frameSizeLimit * 2) throw Error("frameSizeLimit exceeded")
                return "msg,${q.toHexString()}"
            }

            "msg" -> {
                if (ne == null) throw Error("Negentropy not created for this Node")
                if (items.size < 2) throw Error("Message not provided")
                val q = items[1]
                val result = ne.reconcile(q.hexToByteArray())

                if (ne.frameSizeLimit > 0 && result.msg != null && result.msg!!.size > ne.frameSizeLimit * 2)
                    throw Error("frameSizeLimit exceeded")

                return if (result.msg == null) {
                    "done"
                } else {
                    "msg,${result.msg!!.toHexString()}"
                }
            }

            else -> throw Error("unknown cmd: ${items[0]}")
        }

        return null
    }

    @Test
    fun runFullInstructions() {
        val instructions = loadFiles("/multiplerounds-node1.txt", "/multiplerounds-node2.txt")
        val nodes = mutableMapOf<String, Negentropy>()
        var expectedCommand: String? = null

        instructions.forEach {
            if (expectedCommand != null) {
                assertEquals(expectedCommand, it.command)
                expectedCommand = null
            }

            expectedCommand = parseLine(it, nodes)
        }
    }
}