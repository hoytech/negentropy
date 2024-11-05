package com.vitorpamplona.negentropy.message

interface IByteArrayConsumer {
    fun length(): Int
    fun getByte(): Byte
    fun getBytes(n: Int): ByteArray
}

class ByteArrayConsumer(buffer: ByteArray? = null) : IByteArrayConsumer {
    private var _raw: ByteArray = buffer?.copyOf() ?: ByteArray(0)

    override fun length() = _raw.size

    override fun getByte(): Byte {
        return getBytes(1)[0]
    }

    override fun getBytes(n: Int): ByteArray {
        if (length() < n) throw Error("parse ends prematurely")

        val firstSubarray = _raw.copyOfRange(0, n)
        _raw = _raw.copyOfRange(n, _raw.size)
        return firstSubarray
    }

    fun getBytes(until: (Byte) -> Boolean): ByteArray {
        var index = 0

        while (true) {
            if (until(_raw[index])) break
            index++
        }
        index++

        return getBytes(index)
    }
}