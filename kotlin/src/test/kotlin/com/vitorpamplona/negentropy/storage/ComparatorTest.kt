package com.vitorpamplona.negentropy.storage

import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test

class ComparatorTest {
    @Test
    fun testComparator() {
        val st = StorageVector()

        assertEquals(0, StorageUnit(0).compareTo(StorageUnit(0)))
        assertEquals(0, StorageUnit(1000).compareTo(StorageUnit(1000)))
        assertEquals(-1, StorageUnit(1).compareTo(StorageUnit(2)))
        assertEquals(1, StorageUnit(2).compareTo(StorageUnit(1)))
    }

    @Test
    fun testComparatorByteArray() {
        assertEquals(0,  StorageUnit(0, Id("0000")).compareTo(StorageUnit(0, Id("0000"))))
        assertEquals(-1, StorageUnit(0, Id("0000")).compareTo(StorageUnit(0, Id("0010"))))
        assertEquals(1,  StorageUnit(0, Id("0100")).compareTo(StorageUnit(0, Id("0000"))))
        assertEquals(-1, StorageUnit(0, Id("1111")).compareTo(StorageUnit(0, Id("111100"))))
        assertEquals(1,  StorageUnit(0, Id("111100")).compareTo(StorageUnit(0, Id("1111"))))
    }

    @Test
    fun testComparatorUbyteIssue() {
        assertEquals(-1,  StorageUnit(0, Id("9075")).compareTo(StorageUnit(0, Id("90e2"))))
    }
}