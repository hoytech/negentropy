package com.vitorpamplona.negentropy.storage

interface IStorage {
    fun insert(timestamp: Long, idHex: String)
    fun insert(timestamp: Long, id: Id)

    fun seal()
    fun unseal()
    fun size(): Int

    fun getItem(index: Int): StorageUnit

    fun <T> map(run: (StorageUnit) -> T): List<T>
    fun <T> map(begin: Int, end: Int, run: (StorageUnit) -> T): List<T>
    fun forEach(begin: Int, end: Int, run: (StorageUnit) -> Unit)
    fun iterate(begin: Int, end: Int, shouldContinue: (StorageUnit, Int) -> Boolean)

    fun indexAtOrBeforeBound(bound: Bound, begin: Int, end: Int): Int

    fun findTimestamp(id: Id): Long
}