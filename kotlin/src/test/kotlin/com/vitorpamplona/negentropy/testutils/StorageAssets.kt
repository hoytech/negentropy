package com.vitorpamplona.negentropy.testutils

import com.vitorpamplona.negentropy.storage.StorageVector

object StorageAssets {
    @OptIn(ExperimentalStdlibApi::class)
    fun defaultStorage() = StorageVector().apply {
        insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c")
        insert(1678011278, "39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0")
        insert(1678011279,"abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba")

        seal()
    }


    @OptIn(ExperimentalStdlibApi::class)
    fun storageP1() = StorageVector().apply {
        insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c")
        //insert(1678011278, "39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0")
        insert(1678011279,"abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba")

        seal()
    }
    @OptIn(ExperimentalStdlibApi::class)
    fun storageP2() = StorageVector().apply {
        //storage.insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c")
        insert(1678011278, "39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0")
        insert(1678011279,"abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba")

        seal()
    }

    fun storageClient() = StorageVector().apply { loadFile("/client.txt", this) }

    fun storageServer() = StorageVector().apply { loadFile("/server.txt", this) }

    fun storageClientWLimits() = StorageVector().apply { loadFile("/clientWLimits.txt", this) }

    fun storageServerWLimits() = StorageVector().apply { loadFile("/serverWLimits.txt", this) }
}