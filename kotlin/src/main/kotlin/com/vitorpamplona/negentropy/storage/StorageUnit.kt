package com.vitorpamplona.negentropy.storage

val ID_ZERO = Id(ByteArray(0))

class StorageUnit(
    val timestamp: Long,
    val id: Id = ID_ZERO
): Comparable<StorageUnit> {
    override fun compareTo(other: StorageUnit): Int {
        return if (timestamp == other.timestamp) {
            id.compareTo(other.id)
        } else {
            timestamp.compareTo(other.timestamp)
        }
    }
}