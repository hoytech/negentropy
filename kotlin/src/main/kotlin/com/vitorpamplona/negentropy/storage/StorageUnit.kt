package com.vitorpamplona.negentropy.storage

class StorageUnit(
    val timestamp: Long,
    val id: Id
) : Comparable<StorageUnit> {
    override fun compareTo(other: StorageUnit): Int {
        return if (timestamp == other.timestamp) {
            id.compareTo(other.id)
        } else {
            timestamp.compareTo(other.timestamp)
        }
    }
}