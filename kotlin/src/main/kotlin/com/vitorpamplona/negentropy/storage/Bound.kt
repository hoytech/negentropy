package com.vitorpamplona.negentropy.storage

val BOUND_ZERO = HashedByteArray(ByteArray(0))

class Bound(
    val timestamp: Long,
    val prefix: HashedByteArray = BOUND_ZERO
) : Comparable<Bound> {
    init {
        require(prefix.bytes.size <= Id.SIZE) { "bound prefix too long. Expected ${Id.SIZE}, found ${prefix.bytes.size}" }
    }

    override fun compareTo(other: Bound): Int {
        return if (timestamp == other.timestamp) {
            prefix.compareTo(other.prefix)
        } else {
            timestamp.compareTo(other.timestamp)
        }
    }

    internal fun toStorage() = StorageUnit(timestamp, Id(prefix.bytes, true))
}