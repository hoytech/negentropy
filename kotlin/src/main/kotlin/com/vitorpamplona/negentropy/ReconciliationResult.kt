package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.storage.Id

@OptIn(ExperimentalStdlibApi::class)
class ReconciliationResult(
    val msg: ByteArray?,
    val sendIds: List<Id>,
    val needIds: List<Id>
) {
    fun msgToString() = msg?.toHexString()
    fun sendsToString() = sendIds.joinToString(", ") { it.toHexString() }
    fun needsToString() = needIds.joinToString(", ") { it.toHexString() }
}