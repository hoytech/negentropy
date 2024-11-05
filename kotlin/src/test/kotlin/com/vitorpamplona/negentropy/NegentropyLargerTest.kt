package com.vitorpamplona.negentropy

import com.vitorpamplona.negentropy.storage.IStorage
import com.vitorpamplona.negentropy.storage.StorageVector
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class NegentropyLargerTest {
    @OptIn(ExperimentalStdlibApi::class)
    fun load(file: String, db: IStorage) {
        val input1 = object {}.javaClass.getResourceAsStream(file) ?: error("Cannot open/find the file")
        input1.bufferedReader().lines().forEach {
            if (it.contains(",")) {
                val (timestamp, hex) = it.split(",")

                timestamp.toLongOrNull()?.let {
                    db.insert(it, hex.hexToByteArray())
                }
            }
        }

        db.seal()
    }

    fun storageP1() = StorageVector().apply { load("/client.txt", this) }

    fun storageP2() = StorageVector().apply { load("/server.txt", this) }

    @OptIn(ExperimentalStdlibApi::class)
    @Test
    fun testReconcile() {
        val clientDB = storageP1()
        val serverDB = storageP2()

        assertEquals(30, clientDB.size())
        assertEquals(30, serverDB.size())

        val neClient = Negentropy(clientDB)
        val neServer = Negentropy(serverDB)

        val init = neClient.initiate()

        assertEquals("610000021eca047f2a40edb443bab8dc31929f8e5457d9deac5415c287a090a28a2efdf63b9a92950a4498c11158d8b084be8048b38b78b18351dda2f41c278338d39204d2f8c2e717ae854234abcd89b852002d52c52101edd690ae305f61d6215ccfd114b3489cf0594b805a4a45c25327b12ac7c932a02027d36e5f37f8cb53c37d47e7774ee3ba38031494956d5a68822151693f61f2be3c81c376ee162e8dc447bea0bb5c51baa893adb59c4928cc3fa21a568f3e806de3e51647a21a5ea46c4edb38fff4462dcdaaba80cc085c529263c2b1a1435d07c06b8ee5f4adbfd459818540df146798366d0f399fedc8aaec88c9c80ce935ebf6dc19321ca4d48c9f0bc16e72527ebd1c5c95354693c3b0d6fc187ba8d02538ce9570ba0d174d67a1e659f1c7e469248b8451c3970a0cbb72c0b233688759720c3e5ca111e7e37bf11722cd71365dfcf3378bb07164560675874f08db063eb4714dee05c77fa5f8d6ed05e35f157a942e99dd29888565428ed0a41dfb714fee45ac4558fc410412044316bf6796ba8d1a9207251d6652ee56a66a92ad680d5a3f118fdff3eb7710ab45f18a4d1eff6693caef73f3f32eb142f77eee297d29d3b3180d913106d9f53c3c030a7e52df6eec97c58ad25259ec30d30fdef43a626c323208a8a24d1f6cdf02104871feb8cdfe5cb5e3a61550b6d41a395f1537721ce7f831cef2ed270390c28785840a315842f55947f978ce713189e59dc9e26e5ffa12ce73fdf53076fd43e490601ea4503a2ebc95c162ce4c03969ca966a57087cd42cd6e2460bfc622870c2ff971e429278b863127ef97354cc47a1f394fbb2aaa58d2c8ed3d2427b71cefe38406d9844fb8f5a5baf3a5cd7478afa6e43d7088b4416dc2013a53fa3447ee95ed3784f35f4029450f498d8c3f6acc61d90c7706f77ca1f4f75240f49aafe9196a05af1884967975a30a83c96915abe34b8b24a0b2cc91791f4207e526be75eab902a1aebb9d1ddb777d1446f8dd6a6f1116d0c3c9836edbc69eb0d3aeebe4c091235f37e2f90794f989ed15bae2c1f733fe402f022ba54c4e3fd370aa1c0aff93246d67348837e094593d01e9c734c0965f278b9c7594d2be9586568e391dd4d966df16c619a5349d5b2d0125486f7d25e231ea1d546dd453513e8fab3be61030e6ade789d53f16d953ea47535c171800a779b540d7f5f06d97c0d903ce336d6f8d146c74f5574840e8d82897a757da4b67774eafdd4bc5fdfb3ec6febfccf5451fc9214992bd951527eecd9ad2b01647c0ceeae55dda7ca234bf573c6d5e2d5144bfaa00b30f7ce4219af75e452c1389ddf2a626390b3f5c96b9c59e5d9f5b", init.toHexString())

        // send to the server.
        var result = neServer.reconcile(init)

        assertEquals("610000021eca047f2a40edb443bab8dc31929f8e5457d9deac5415c287a090a28a2efdf63b9a92950a4498c11158d8b084be8048b38b78b18351dda2f41c278338d39204d2b3489cf0594b805a4a45c25327b12ac7c932a02027d36e5f37f8cb53c37d47e7774ee3ba38031494956d5a68822151693f61f2be3c81c376ee162e8dc447bea0ef3e606c1b9291b7ba1eaa119b431763ae8a41c0da9f60c8df81e6cb85f8fc75bb5c51baa893adb59c4928cc3fa21a568f3e806de3e51647a21a5ea46c4edb38fff4462dcdaaba80cc085c529263c2b1a1435d07c06b8ee5f4adbfd459818540df146798366d0f399fedc8aaec88c9c80ce935ebf6dc19321ca4d48c9f0bc16e3ada797b897747c1f6a389025bc03aba5c706f22f7bd60e4a2d9dabcfe21c32472527ebd1c5c95354693c3b0d6fc187ba8d02538ce9570ba0d174d67a1e659f1c7e469248b8451c3970a0cbb72c0b233688759720c3e5ca111e7e37bf11722cd71365dfcf3378bb07164560675874f08db063eb4714dee05c77fa5f8d6ed05e35f157a942e99dd29888565428ed0a41dfb714fee45ac4558fc410412044316bf6796ba8d1a9207251d6652ee56a66a92ad680d5a3f118fdff3eb7710ab45f18a7d929283afc8830733b84953fcc09a6b7898581cb557aad4e0c0d7382ba7f1a74d1eff6693caef73f3f32eb142f77eee297d29d3b3180d913106d9f53c3c030a556ea7b8a8e4c8ffadea26e689c7d1fb24e83873e839fa3f94c630ccd5b5567e7e52df6eec97c58ad25259ec30d30fdef43a626c323208a8a24d1f6cdf021048840a315842f55947f978ce713189e59dc9e26e5ffa12ce73fdf53076fd43e490601ea4503a2ebc95c162ce4c03969ca966a57087cd42cd6e2460bfc622870c2ff971e429278b863127ef97354cc47a1f394fbb2aaa58d2c8ed3d2427b71cefe36a05af1884967975a30a83c96915abe34b8b24a0b2cc91791f4207e526be75eab902a1aebb9d1ddb777d1446f8dd6a6f1116d0c3c9836edbc69eb0d3aeebe4c091235f37e2f90794f989ed15bae2c1f733fe402f022ba54c4e3fd370aa1c0aff93246d67348837e094593d01e9c734c0965f278b9c7594d2be9586568e391dd4d966df16c619a5349d5b2d0125486f7d25e231ea1d546dd453513e8fab3be61030e6ade789d53f16d953ea47535c171800a779b540d7f5f06d97c0d903ce336dc820f4c1784abda82e640fc20cbfe0f7f0544da4cfe74d382392d73375d2503d5650fc157cb1c947b00cee120fd71b6e93dc6dfdb87f092f3d0c52b31d74675c451fc9214992bd951527eecd9ad2b01647c0ceeae55dda7ca234bf573c6d5e2d", result.msgToString())
        assertEquals("", result.sendsToString())
        assertEquals("", result.needsToString())

        // back to client
        result = neClient.reconcile(result.msg!!)

        assertEquals(null, result.msg)

        // client should send
        var index = 0
        assertEquals("f8c2e717ae854234abcd89b852002d52c52101edd690ae305f61d6215ccfd114", result.sendIds[index++].toHexString())
        assertEquals("71feb8cdfe5cb5e3a61550b6d41a395f1537721ce7f831cef2ed270390c28785", result.sendIds[index++].toHexString())
        assertEquals("8406d9844fb8f5a5baf3a5cd7478afa6e43d7088b4416dc2013a53fa3447ee95", result.sendIds[index++].toHexString())
        assertEquals("ed3784f35f4029450f498d8c3f6acc61d90c7706f77ca1f4f75240f49aafe919", result.sendIds[index++].toHexString())
        assertEquals("6f8d146c74f5574840e8d82897a757da4b67774eafdd4bc5fdfb3ec6febfccf5", result.sendIds[index++].toHexString())
        assertEquals("5144bfaa00b30f7ce4219af75e452c1389ddf2a626390b3f5c96b9c59e5d9f5b", result.sendIds[index++].toHexString())
        assertEquals(6, result.needIds.size)

        // client should ask
        index = 0
        assertEquals("5650fc157cb1c947b00cee120fd71b6e93dc6dfdb87f092f3d0c52b31d74675c", result.needIds[index++].toHexString())
        assertEquals("3ada797b897747c1f6a389025bc03aba5c706f22f7bd60e4a2d9dabcfe21c324", result.needIds[index++].toHexString())
        assertEquals("c820f4c1784abda82e640fc20cbfe0f7f0544da4cfe74d382392d73375d2503d", result.needIds[index++].toHexString())
        assertEquals("556ea7b8a8e4c8ffadea26e689c7d1fb24e83873e839fa3f94c630ccd5b5567e", result.needIds[index++].toHexString())
        assertEquals("7d929283afc8830733b84953fcc09a6b7898581cb557aad4e0c0d7382ba7f1a7", result.needIds[index++].toHexString())
        assertEquals("ef3e606c1b9291b7ba1eaa119b431763ae8a41c0da9f60c8df81e6cb85f8fc75", result.needIds[index++].toHexString())
        assertEquals(6, result.needIds.size)

        result.sendIds.forEach {
            serverDB.unseal()
            serverDB.insert(clientDB.findTimestamp(it), it)
            serverDB.seal()
        }

        result.needIds.forEach {
            clientDB.unseal()
            clientDB.insert(serverDB.findTimestamp(it), it)
            clientDB.seal()
        }

        // second round

        val neClient2 = Negentropy(clientDB)
        val init2 = neClient2.initiate()

        assertEquals("610000021eca047f2a40edb443bab8dc31929f8e5457d9deac5415c287a090a28a2efdf63b9a92950a4498c11158d8b084be8048b38b78b18351dda2f41c278338d39204d2f8c2e717ae854234abcd89b852002d52c52101edd690ae305f61d6215ccfd114b3489cf0594b805a4a45c25327b12ac7c932a02027d36e5f37f8cb53c37d47e7774ee3ba38031494956d5a68822151693f61f2be3c81c376ee162e8dc447bea0bb5c51baa893adb59c4928cc3fa21a568f3e806de3e51647a21a5ea46c4edb38fff4462dcdaaba80cc085c529263c2b1a1435d07c06b8ee5f4adbfd459818540df146798366d0f399fedc8aaec88c9c80ce935ebf6dc19321ca4d48c9f0bc16e72527ebd1c5c95354693c3b0d6fc187ba8d02538ce9570ba0d174d67a1e659f1c7e469248b8451c3970a0cbb72c0b233688759720c3e5ca111e7e37bf11722cd71365dfcf3378bb07164560675874f08db063eb4714dee05c77fa5f8d6ed05e35f157a942e99dd29888565428ed0a41dfb714fee45ac4558fc410412044316bf6796ba8d1a9207251d6652ee56a66a92ad680d5a3f118fdff3eb7710ab45f18a4d1eff6693caef73f3f32eb142f77eee297d29d3b3180d913106d9f53c3c030a7e52df6eec97c58ad25259ec30d30fdef43a626c323208a8a24d1f6cdf02104871feb8cdfe5cb5e3a61550b6d41a395f1537721ce7f831cef2ed270390c28785840a315842f55947f978ce713189e59dc9e26e5ffa12ce73fdf53076fd43e490601ea4503a2ebc95c162ce4c03969ca966a57087cd42cd6e2460bfc622870c2ff971e429278b863127ef97354cc47a1f394fbb2aaa58d2c8ed3d2427b71cefe38406d9844fb8f5a5baf3a5cd7478afa6e43d7088b4416dc2013a53fa3447ee95ed3784f35f4029450f498d8c3f6acc61d90c7706f77ca1f4f75240f49aafe9196a05af1884967975a30a83c96915abe34b8b24a0b2cc91791f4207e526be75eab902a1aebb9d1ddb777d1446f8dd6a6f1116d0c3c9836edbc69eb0d3aeebe4c091235f37e2f90794f989ed15bae2c1f733fe402f022ba54c4e3fd370aa1c0aff93246d67348837e094593d01e9c734c0965f278b9c7594d2be9586568e391dd4d966df16c619a5349d5b2d0125486f7d25e231ea1d546dd453513e8fab3be61030e6ade789d53f16d953ea47535c171800a779b540d7f5f06d97c0d903ce336d6f8d146c74f5574840e8d82897a757da4b67774eafdd4bc5fdfb3ec6febfccf5451fc9214992bd951527eecd9ad2b01647c0ceeae55dda7ca234bf573c6d5e2d5144bfaa00b30f7ce4219af75e452c1389ddf2a626390b3f5c96b9c59e5d9f5b", init.toHexString())

        // send to the server.
        result = neServer.reconcile(init2)

        assertEquals("61", result.msgToString())
        assertEquals("", result.sendsToString())
        assertEquals("", result.needsToString())

        // back to client
        result = neClient2.reconcile(result.msg!!)

        assertEquals(null, result.msg)
        assertEquals("", result.sendsToString())
        assertEquals("", result.needsToString())
    }
}