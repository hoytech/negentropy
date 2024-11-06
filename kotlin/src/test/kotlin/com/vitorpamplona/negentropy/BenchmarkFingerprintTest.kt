package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.fingerprint.FingerprintCalculator
import com.vitorpamplona.negentropy.storage.StorageVector
import com.vitorpamplona.negentropy.testutils.StorageAssets.storageFrameLimitsClient
import com.vitorpamplona.negentropy.testutils.StorageAssets.storageFrameLimitsServer
import com.vitorpamplona.negentropy.testutils.benchmark
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class BenchmarkFingerprintTest {
    @Test
    fun benchmarkFingerprint() {
        benchmark("JIT Benchmark") { 1+1 }

        val storage = StorageVector().apply {
            insert(1678011277, "eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c")
            insert(1678011278, "39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0")
            insert(1678011279, "abc81d58ebe3b9a87100d47f58bf15e9b1cbf62d38623f11d0f0d17179f5f3ba")

            seal()
        }

        benchmark("Fingerprint Canculator") { FingerprintCalculator().run(storage, 0, 3) }
    }
}