package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.storage.StorageVector
import java.io.PrintStream
import java.util.*

val DEBUG = false

fun initDebug(owner: Long) {
    if (DEBUG) {
        System.setErr(PrintStream("err$owner.txt"));
    }
}

fun debug(owner: Long, message: String) {
    if (DEBUG) {
        val time = System.nanoTime()
        System.err.println("$time,$owner,$message")
    }
}

@OptIn(ExperimentalStdlibApi::class)
fun main() {
    val owner = System.nanoTime()

    initDebug(owner)

    val frameSizeLimitStr = System.getenv("FRAMESIZELIMIT") ?: "0"
    val frameSizeLimit = frameSizeLimitStr.toLongOrNull() ?: 0
    val scanner = Scanner(System.`in`)

    var ne: Negentropy? = null
    val storage = StorageVector()

    debug(owner, "create,$frameSizeLimitStr")

    while (scanner.hasNextLine()) {
        val line = scanner.nextLine()

        debug(owner, line)

        val items = line.split(",")

        when (items[0]) {
            "item" -> {
                if (items.size != 3) throw Error("too few items")
                val created = items[1].toLongOrNull() ?: throw Error("Invalid timestamp format")
                storage.insert(created, items[2].trim())
            }

            "seal" -> {
                storage.seal()
                ne = Negentropy(storage, frameSizeLimit)
            }

            "initiate" -> {
                if (ne == null) throw Error("Negentropy not created")
                val q = ne.initiate()
                if (frameSizeLimit > 0 && q.size > frameSizeLimit * 2) throw Error("frameSizeLimit exceeded")
                println("msg,${q.toHexString()}")
            }

            "msg" -> {
                if (ne == null) throw Error("Negentropy not created")
                if (items.size < 2) throw Error("Message not provided")

                val result = ne.reconcile(items[1].hexToByteArray())

                result.sendIds.forEach { id -> println("have,${id.toHexString()}") }
                result.needIds.forEach { id -> println("need,${id.toHexString()}") }

                if (frameSizeLimit > 0 && result.msg != null && result.msg.size > frameSizeLimit * 2) {
                    throw Error("frameSizeLimit exceeded")
                }

                if (result.msg == null) {
                    println("done")
                } else {
                    println("msg,${result.msg.toHexString()}")
                }
            }

            else -> throw Error("unknown cmd: ${items[0]}")
        }
    }
}