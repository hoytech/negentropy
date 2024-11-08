package com.vitorpamplona.negentropy.storage

import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class OrderTest {
    @Test
    fun testOrderBetweenSameDates() {
        val storage = StorageVector()

        storage.insert(1677970539,"b3489cf0594b805a4a45c25327b12ac7c932a02027d36e5f37f8cb53c37d47e7")
        storage.insert(1677970539,"774ee3ba38031494956d5a68822151693f61f2be3c81c376ee162e8dc447bea0")
        storage.insert(1677970539,"ef3e606c1b9291b7ba1eaa119b431763ae8a41c0da9f60c8df81e6cb85f8fc75")

        storage.seal()

        assertEquals("774ee3ba38031494956d5a68822151693f61f2be3c81c376ee162e8dc447bea0", storage.getItem(0).id.toHexString())
        assertEquals("b3489cf0594b805a4a45c25327b12ac7c932a02027d36e5f37f8cb53c37d47e7", storage.getItem(1).id.toHexString())
        assertEquals("ef3e606c1b9291b7ba1eaa119b431763ae8a41c0da9f60c8df81e6cb85f8fc75", storage.getItem(2).id.toHexString())
    }

    @Test
    fun testOrderBetweenSameDates2() {
        val storage = StorageVector()

        storage.insert(1677981604,"8971540703e711ae8a12444aa96e492d130fde2a8d1e50059e1cf8a43badaa19")
        storage.insert(1677981604,"90e29d9b1c37333a25ad02a478cf356e493da355490baa860cfb16c9bd067cda")
        storage.insert(1677981604,"907530c3ae8c3a3066d0238119dc595350649f35cafffa9a90d4868361f04cff")

        storage.seal()

        assertEquals("8971540703e711ae8a12444aa96e492d130fde2a8d1e50059e1cf8a43badaa19", storage.getItem(0).id.toHexString())
        assertEquals("907530c3ae8c3a3066d0238119dc595350649f35cafffa9a90d4868361f04cff", storage.getItem(1).id.toHexString())
        assertEquals("90e29d9b1c37333a25ad02a478cf356e493da355490baa860cfb16c9bd067cda", storage.getItem(2).id.toHexString())
    }
}