package com.vitorpamplona.negentropy.message

interface IByteArrayBuilder {
    fun length(): Int
    fun extend(buf: ByteArrayBuilder)
    fun extend(newBuffer: ByteArray)
    fun unwrap(): ByteArray
}

class ByteArrayBuilder : IByteArrayBuilder {
    private var _raw: ByteArray = ByteArray(0)

    override fun length() = _raw.size

    override fun unwrap() = _raw.copyOf()

    override fun extend(buf: ByteArrayBuilder) { _raw += buf._raw }

    override fun extend(newBuffer: ByteArray) { _raw += newBuffer }
}