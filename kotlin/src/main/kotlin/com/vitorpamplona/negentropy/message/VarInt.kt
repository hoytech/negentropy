package com.vitorpamplona.negentropy.message

fun isEndVarInt(byte: Int) = ((byte and 128) == 0)

fun decodeVarInt(bytes: ByteArray): Long {
    var res = 0L

    bytes.forEach {
        res = (res shl 7) or (it.toLong() and 127)
    }

    return res
}

fun encodeVarInt(n: Int): ByteArray {
    if (n == 0) return byteArrayOf(0)

    val list = mutableListOf<Int>()

    var number = n
    while (number != 0) {
        list.add(number and 127)
        number = number ushr 7
    }

    list.reverse()

    for (i in 0 until list.size - 1) list[i] = list[i] or 128

    return ByteArray(list.size) {
        list[it].toByte()
    }
}

fun encodeVarInt(n: Long): ByteArray {
    if (n == 0L) return byteArrayOf(0)

    val list = mutableListOf<Long>()

    var number = n
    while (number != 0L) {
        list.add(number and 127)
        number = number ushr 7
    }

    list.reverse()

    for (i in 0 until list.size - 1) list[i] = list[i] or 128

    return ByteArray(list.size) {
        list[it].toByte()
    }
}