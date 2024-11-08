package com.vitorpamplona.negentropy.storage

class Id : HashedByteArray {
    companion object {
        const val SIZE = 32
    }

    constructor(byteArray: ByteArray, ignoreCheck: Boolean = false) : super(byteArray) {
        if (!ignoreCheck) {
            check(bytes.size == SIZE) { "Id with invalid size. Expected $SIZE, found ${bytes.size}" }
        }
    }

    constructor(hex: String) : super(hex) {
        check(bytes.size == SIZE) { "Id with invalid size. Expected $SIZE, found ${bytes.size}" }
    }

    fun sharedPrefixPlusMyNextChar(other: Id): HashedByteArray {
        var sharedPrefixBytes = 0
        for (i in 0 until SIZE) {
            if (bytes[i] != other.bytes[i]) break
            sharedPrefixBytes++
        }

        return HashedByteArray(bytes.copyOfRange(0, sharedPrefixBytes + 1))
    }
}