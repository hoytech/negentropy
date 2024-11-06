package com.vitorpamplona.negentropy.testutils

import kotlin.time.measureTimedValue

fun <T> benchmarkOnce(desc: String, func: () -> T): T {
    val (value, elapsed) = measureTimedValue {
        func()
    }

    println("${elapsed.div(5)} - $desc")

    return value
}

fun <T> benchmark(desc: String, func: () -> T): T {
    func()

    val (value, elapsed) = measureTimedValue {
        func()
        func()
        func()
        func()
        func()
    }

    println("${elapsed.div(5)} - $desc")

    return value
}