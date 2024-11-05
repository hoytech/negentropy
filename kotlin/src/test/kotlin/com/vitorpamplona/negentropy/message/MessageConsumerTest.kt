package com.vitorpamplona.negentropy.message

import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class MessageConsumerTest {
    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testTimestampInDecoder() {
        val operator = MessageConsumer("86a091d70e0202".hexToByteArray())

        assertEquals(1678011277, operator.decodeTimestampIn())
        assertEquals(1678011278, operator.decodeTimestampIn())
        assertEquals(1678011279, operator.decodeTimestampIn())
    }

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testBoundDecoder() {
        val operator = MessageConsumer("86a091d70e20eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c".hexToByteArray())

        val bound = operator.decodeBound()

        assertEquals(1678011277, bound.timestamp)
        assertEquals("eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c", bound.id.toHexString())
    }
}