package com.vitorpamplona.negentropy.storage

import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test

class ComparatorTest {
    @Test
    fun testComparator() {
        val st = StorageVector()

        assertEquals(0, Bound(0).compareTo(Bound(0)))
        assertEquals(0, Bound(1000).compareTo(Bound(1000)))
        assertEquals(-1, Bound(1).compareTo(Bound(2)))
        assertEquals(1, Bound(2).compareTo(Bound(1)))
    }

    @Test
    fun testComparatorByteArray() {
        assertEquals(0,  Bound(0, HashedByteArray("0000")).compareTo(Bound(0, HashedByteArray("0000"))))
        assertEquals(-1, Bound(0, HashedByteArray("0000")).compareTo(Bound(0, HashedByteArray("0010"))))
        assertEquals(1,  Bound(0, HashedByteArray("0100")).compareTo(Bound(0, HashedByteArray("0000"))))
        assertEquals(-1, Bound(0, HashedByteArray("1111")).compareTo(Bound(0, HashedByteArray("111100"))))
        assertEquals(1,  Bound(0, HashedByteArray("111100")).compareTo(Bound(0, HashedByteArray("1111"))))
    }

    @Test
    fun testComparatorUbyteIssue() {
        assertEquals(-1,  Bound(0, HashedByteArray("9075")).compareTo(Bound(0, HashedByteArray("90e2"))))
    }
}