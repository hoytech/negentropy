package com.vitorpamplona.negentropy.storage

/**
 * Creates an ByteArray class that implements hashcode and equals of byte arrays
 * to be used by java collections. This implementation caches the value of the
 * hashcode to speed up processing.
 *
 * This **assumes** the byte array will never change
 */
open class HashedByteArray : Comparable<HashedByteArray> {
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
        var hash = 1
        for (i in bytes.size - 1 downTo 0)
            hash = 31 * hash + bytes[i].toInt()
        return hash
    }

    override fun hashCode() = hashCode

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        return equalsId(other as HashedByteArray)
    }

    fun equalsId(other: HashedByteArray): Boolean {
        // check hashcode first for speed
        if (hashCode != other.hashCode) return false

        // if hashcode matches, check content.
        if (!bytes.contentEquals(other.bytes)) return false

        return true
    }

    override fun compareTo(other: HashedByteArray): Int {
        for (i in bytes.indices) {
            if (i >= other.bytes.size) return 1 // `a` is longer

            // avoids conversion to unsigned byte if the same
            if (bytes[i] == other.bytes[i]) continue

            // avoids conversion to unsigned byte if both are negative
            if (bytes[i] < 0 && other.bytes[i] < 0) {
                if (bytes[i] < other.bytes[i]) return -1
                if (bytes[i] > other.bytes[i]) return 1
            }

            if (bytes[i] < 0) return 1 // other.bytes[i] is positive or zero

            if (other.bytes[i] < 0) return -1 // bytes[i] is positive or zero

            // both are positive
            if (bytes[i] < other.bytes[i]) return -1
            if (bytes[i] > other.bytes[i]) return 1
        }
        return when {
            bytes.size > other.bytes.size -> 1  // `a` is longer
            bytes.size < other.bytes.size -> -1 // `b` is longer
            else -> 0            // Both arrays are equal
        }
    }

    @OptIn(ExperimentalStdlibApi::class)
    fun toHexString() = bytes.toHexString()
}