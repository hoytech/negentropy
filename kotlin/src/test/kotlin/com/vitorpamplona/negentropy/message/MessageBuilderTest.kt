package com.vitorpamplona.negentropy.message

import com.vitorpamplona.negentropy.storage.Id
import com.vitorpamplona.negentropy.storage.StorageUnit
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class MessageBuilderTest {
    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testTimestampOutEncoder() {
        val operator = MessageBuilder()

        assertEquals("86a091d70e", operator.encodeTimestampOut(1678011277).toHexString())
        assertEquals("02", operator.encodeTimestampOut(1678011278).toHexString())
        assertEquals("02", operator.encodeTimestampOut(1678011279).toHexString())
        assertEquals("02", operator.encodeTimestampOut(1678011280).toHexString())
    }

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testBoundEncoder() {
        val test = StorageUnit(1678011277, Id("eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c"))
        val operator = MessageBuilder()
        operator.addBound(test)

        assertEquals("86a091d70e20eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c", operator.toByteArray().toHexString())
    }
}