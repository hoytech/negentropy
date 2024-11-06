package com.vitorpamplona.negentropy.testutils

import com.vitorpamplona.negentropy.Negentropy
import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageVector

class InstructionParser {
    class Instruction(
        val toNode: String,
        val time: Long,
        val command: Command,
    )

    sealed class Command(val source: String) {
        class Create(source: String, val frameSizeLimit: Long) : Command(source)
        class Item(source: String, val created: Long, val id: Id) :Command(source)
        class Seal(source: String) : Command(source)
        class Initiate(source: String) : Command(source)
        class Message(source: String, val msg: ByteArray) : Command(source)
    }

    @OptIn(ExperimentalStdlibApi::class)
    private fun parseCommand(command: String): Command {
        val items = command.split(",")
        return when (items[0]) {
            "create" -> {
                if (items.size != 2) throw Error("too few items")
                val frameSizeLimit = items[1].toLongOrNull() ?: throw Error("Invalid framesoze format")
                Command.Create(command, frameSizeLimit)
            }

            "item" -> {
                if (items.size != 3) throw Error("too few items")
                val created = items[1].toLongOrNull() ?: throw Error("Invalid timestamp format")
                val id = items[2].trim()
                Command.Item(command, created, Id(id))
            }

            "seal" -> Command.Seal(command)
            "initiate" -> Command.Initiate(command)
            "msg" -> {
                if (items.size < 2) throw Error("Message not provided")
                val q = items[1]
                Command.Message(command, q.hexToByteArray())
            }
            else -> throw Error("unknown cmd: ${items[0]}")
        }
    }

    private fun loadFile(file: String): List<Instruction> {
        val input = object {}.javaClass.getResourceAsStream(file) ?: error("Cannot open/find the file $file")
        val list = mutableListOf<Instruction>()

        input.bufferedReader().lines().forEach {
            if (it.contains(",")) {
                val (time, toNode, command) = it.split(",", limit = 3)
                list.add(Instruction(toNode, time.toLong(), parseCommand(command)))
            }
        }

        return list
    }

    fun loadFiles(file1: String, file2: String): List<Instruction> {
        return (loadFile(file1) + loadFile(file2)).sortedBy { it.time }
    }

    @OptIn(ExperimentalStdlibApi::class)
    fun runLine(line: Instruction, nodes: MutableMap<String, Negentropy>): String? {
        val ne = nodes[line.toNode]
        when (val command = line.command) {
            is Command.Create -> {
                check(!nodes.contains(line.toNode)) { "Node already exist" }

                nodes[line.toNode] = Negentropy(StorageVector(), command.frameSizeLimit)
            }
            is Command.Item -> {
                check(ne != null) { "Negentropy not created for this Node ${line.toNode}" }
                ne.insert(command.created, command.id)
            }

            is Command.Seal -> {
                check(ne != null) { "Negentropy not created for this Node ${line.toNode}" }
                ne.seal()
            }

            is Command.Initiate -> {
                check(ne != null) { "Negentropy not created for this Node ${line.toNode}" }
                val q = ne.initiate()
                if (ne.frameSizeLimit > 0 && q.size > ne.frameSizeLimit * 2) throw Error("frameSizeLimit exceeded")
                return "msg,${q.toHexString()}"
            }

            is Command.Message -> {
                check(ne != null) { "Negentropy not created for this Node ${line.toNode}" }
                val result = ne.reconcile(command.msg)

                if (ne.frameSizeLimit > 0 && result.msg != null && result.msg!!.size > ne.frameSizeLimit * 2)
                    throw Error("frameSizeLimit exceeded")

                return if (result.msg == null) {
                    "done"
                } else {
                    "msg,${result.msg!!.toHexString()}"
                }
            }
        }

        return null
    }
}