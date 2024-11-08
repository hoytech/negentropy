package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.storage.Bound
import com.vitorpamplona.negentropy.storage.Id

sealed class Mode(
    val nextBound: Bound,
) {
    class Skip(nextBound: Bound) : Mode(nextBound) {
        companion object {
            const val CODE = 0
        }
    }

    class Fingerprint(nextBound: Bound, val fingerprint: com.vitorpamplona.negentropy.fingerprint.Fingerprint) :
        Mode(nextBound) {
        companion object {
            const val CODE = 1
        }
    }

    class IdList(nextBound: Bound, val ids: List<Id>) : Mode(nextBound) {
        companion object {
            const val CODE = 2
        }
    }
}

