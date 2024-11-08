package com.vitorpamplona.negentropy.storage

import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class ComparatorTest {
    @Test
    fun testComparator() {
        val st = StorageVector()

        assertEquals(0, st.itemCompare(StorageUnit(0), StorageUnit(0)))
        assertEquals(0, st.itemCompare(StorageUnit(1000), StorageUnit(1000)))
        assertEquals(-1, st.itemCompare(StorageUnit(1), StorageUnit(2)))
        assertEquals(1, st.itemCompare(StorageUnit(2), StorageUnit(1)))
    }

    @Test
    fun testComparatorByteArray() {
        val st = StorageVector()

        assertEquals(0,  st.itemCompare(StorageUnit(0, Id("0000")), StorageUnit(0, Id("0000"))))
        assertEquals(-1, st.itemCompare(StorageUnit(0, Id("0000")), StorageUnit(0, Id("0010"))))
        assertEquals(1,  st.itemCompare(StorageUnit(0, Id("0100")), StorageUnit(0, Id("0000"))))
        assertEquals(-1, st.itemCompare(StorageUnit(0, Id("1111")), StorageUnit(0, Id("111100"))))
        assertEquals(1,  st.itemCompare(StorageUnit(0, Id("111100")), StorageUnit(0, Id("1111"))))
    }

    @Test
    fun testComparatorUbyteIssue() {
        val st = StorageVector()

        assertEquals(-1,  st.itemCompare(StorageUnit(0, Id("9075")), StorageUnit(0, Id("90e2"))))
    }
}