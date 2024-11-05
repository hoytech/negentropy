package com.vitorpamplona.negentropy.message

fun isEndVarInt(byte: Byte) = ((byte.toInt() and 128) == 0)

fun decodeVarInt(bytes: ByteArray): Long {
    var res = 0L

    bytes.forEach {
        res = (res shl 7) or (it.toLong() and 127)
    }

    return res
}

fun encodeVarInt(n: Int): ByteArray {
    if (n == 0) return byteArrayOf(0)

    val o = mutableListOf<Int>()

    var number = n
    while (number != 0) {
        o.add(number and 127)
        number = number ushr 7
    }

    o.reverse()

    for (i in 0 until o.size - 1) o[i] = o[i] or 128

    return ByteArray(o.size) {
        o[it].toByte()
    }
}