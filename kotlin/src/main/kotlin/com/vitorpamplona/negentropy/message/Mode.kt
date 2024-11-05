package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.storage.StorageUnit

sealed class Mode(
    val nextBound: StorageUnit,
) {
    class Skip(nextBound: StorageUnit) : Mode(nextBound) {
        companion object {
            const val CODE = 0
        }
    }

    class Fingerprint(nextBound: StorageUnit, val fingerprint: ByteArray) : Mode(nextBound) {
        companion object {
            const val CODE = 1
        }
    }

    class IdList(nextBound: StorageUnit, val ids: List<ByteArray>) : Mode(nextBound) {
        companion object {
            const val CODE = 2
        }
    }
}

