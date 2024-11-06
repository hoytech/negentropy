package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageVector
import java.util.*

@OptIn(ExperimentalStdlibApi::class)
fun main() {
    val frameSizeLimit = System.getenv("FRAMESIZELIMIT")?.toIntOrNull() ?: 0
    val scanner = Scanner(System.`in`)

    var ne: Negentropy? = null
    val storage = StorageVector()

    while (scanner.hasNextLine()) {
        val line = scanner.nextLine()
        val items = line.split(",")

        when (items[0]) {
            "item" -> {
                if (items.size != 3) throw Error("too few items")
                val created = items[1].toLongOrNull() ?: throw Error("Invalid timestamp format")
                val id = items[2].trim()
                storage.insert(created, Id(id.hexToByteArray()))
            }

            "seal" -> {
                storage.seal()
                ne = Negentropy(storage, frameSizeLimit)
            }

            "initiate" -> {
                ne?.let {
                    val q = it.initiate()
                    if (frameSizeLimit > 0 && q.size > frameSizeLimit * 2) throw Error("frameSizeLimit exceeded")
                    println("msg,${q.toHexString()}")
                }
            }

            "msg" -> {
                if (items.size < 2) throw Error("Message not provided")
                val q = items[1]
                ne?.let {
                    val result = it.reconcile(q.hexToByteArray())

                    result.sendIds.forEach { id -> println("have,${id.bytes.toHexString()}") }
                    result.needIds.forEach { id -> println("need,${id.bytes.toHexString()}") }

                    if (frameSizeLimit > 0 && result.msg != null && result.msg.size > frameSizeLimit * 2) throw Error(
                        "frameSizeLimit exceeded"
                    )

                    if (result.msg == null) {
                        println("done")
                    } else {
                        println("msg,${result.msg.toHexString()}")
                    }
                }
            }

            else -> throw Error("unknown cmd: ${items[0]}")
        }
    }
}