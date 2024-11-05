package com.vitorpamplona.negentropy.storage

interface IStorage {
    fun insert(timestamp: Long, id: ByteArray)
    fun seal()
    fun unseal()
    fun size(): Int

    fun getItem(i: Int): StorageUnit

    fun <T> map(begin: Int, end: Int, run: (StorageUnit) -> T): List<T>
    fun forEach(begin: Int, end: Int, run: (StorageUnit) -> Unit)
    fun iterate(begin: Int, end: Int, shouldContinue: (StorageUnit) -> Boolean)

    fun findLowerBound(begin: Int, end: Int, bound: StorageUnit): Int

    fun findTimestamp(it: ByteArray): Long
}