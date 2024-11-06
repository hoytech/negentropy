package com.vitorpamplona.negentropy.testutils

import com.vitorpamplona.negentropy.storage.IStorage
import com.vitorpamplona.negentropy.storage.Id

@OptIn(ExperimentalStdlibApi::class)
fun loadFile(file: String, db: IStorage) {
    val input1 = object {}.javaClass.getResourceAsStream(file) ?: error("Cannot open/find the file")
    input1.bufferedReader().lines().forEach {
        if (it.contains(",")) {
            val (timestamp, hex) = it.split(",")

            timestamp.toLongOrNull()?.let {
                db.insert(it, Id(hex.hexToByteArray()))
            }
        }
    }

    db.seal()
}