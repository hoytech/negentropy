package com.vitorpamplona.negentropy.storage

import com.vitorpamplona.negentropy.ID_SIZE

/**
 * Creates an ID class that implements hashcode and equals of byte arrays
 * to be used by java collections. This implementation caches the value of the
 * hashcode to speed up processing.
 *
 * This **assumes** the byte array will never change
 */
class Id : Comparable<Id> {
    val bytes: ByteArray
    private val hashCode: Int

    constructor(byte: ByteArray) {
        this.bytes = byte
        this.hashCode = computeHashcode()
    }

    @OptIn(ExperimentalStdlibApi::class)
    constructor(idHex: String) {
        this.bytes = idHex.hexToByteArray()
        this.hashCode = computeHashcode()
    }

    /**
     * Guarantees that two arrays with the same content get the same hashcode
     */
    private fun computeHashcode(): Int {
        var h = 1
        for (i in bytes.size - 1 downTo 0)
            h = 31 * h + bytes[i].toInt()
        return h
    }

    override fun hashCode() = hashCode

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as Id

        // check hashcode first for speed
        if (hashCode != other.hashCode) return false

        // if hashcode matches, check content.
        if (!bytes.contentEquals(other.bytes)) return false

        return true
    }

    override fun compareTo(other: Id): Int {
        for (i in bytes.indices) {
            if (i >= other.bytes.size) return 1 // `a` is longer
            if (bytes[i] < other.bytes[i]) return -1
            if (bytes[i] > other.bytes[i]) return 1
        }
        return when {
            bytes.size > other.bytes.size -> 1  // `a` is longer
            bytes.size < other.bytes.size -> -1 // `b` is longer
            else -> 0            // Both arrays are equal
        }
    }

    fun sharedPrefix(other: Id): Id {
        var sharedPrefixBytes = 0
        for (i in 0 until ID_SIZE) {
            if (bytes[i] != other.bytes[i]) break
            sharedPrefixBytes++
        }

        return Id(bytes.copyOfRange(0, sharedPrefixBytes + 1))
    }

    @OptIn(ExperimentalStdlibApi::class)
    fun toHexString() = bytes.toHexString()
}