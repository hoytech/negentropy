package com.vitorpamplona.negentropy.fingerprint

import com.vitorpamplona.negentropy.storage.HashedByteArray

class Fingerprint : HashedByteArray {
    constructor(byte: ByteArray) : super(byte) {
        check(bytes.size == SIZE) { "Wrong size fingerprint" }
    }

    constructor(idHex: String) : super(idHex) {
        check(bytes.size == SIZE) { "Wrong size fingerprint" }
    }

    companion object {
        const val SIZE = 16
    }
}