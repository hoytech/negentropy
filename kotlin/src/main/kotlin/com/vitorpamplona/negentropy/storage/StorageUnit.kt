package com.vitorpamplona.negentropy.storage

val ID_ZERO = Id(ByteArray(0))

class StorageUnit(
    val timestamp: Long,
    val id: Id = ID_ZERO
)