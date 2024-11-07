package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.testutils.InstructionParser
import com.vitorpamplona.negentropy.testutils.benchmark
import org.junit.jupiter.api.Test

class BenchmarkMultipleRoundsTest {
    @Test
    fun benchmark() {
        benchmark("JIT Benchmark") { 1+1 }

        val ip = InstructionParser()
        val instructions = ip.loadFiles("/multiplerounds-node1.txt", "/multiplerounds-node2.txt")

        benchmark("Reconcile 7 Rounds at 60000-byte frame and 100,000 records") {
            val nodes = mutableMapOf<String, InstructionParser.Node>()

            instructions.forEach {
                ip.runLine(it, nodes)
            }
        }
    }
}