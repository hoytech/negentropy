import com.vitorpamplona.negentropy.Negentropy
import com.vitorpamplona.negentropy.testutils.StorageAssets
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals

class NegentropyClientServerTest {
    @Test
    fun testReconcile() {
        val operator1 = Negentropy(StorageAssets.storageP1())
        val init1 = operator1.initiate()

        val operator2 = Negentropy(StorageAssets.storageP2())
        val init2 = operator2.initiate()

        val resultFor1 = operator1.reconcile(init2)

        assertEquals(null, resultFor1.msg)
        assertEquals("39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0", resultFor1.needsToString())
        assertEquals("eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c", resultFor1.sendsToString())

        val resultFor2 = operator2.reconcile(init1)

        assertEquals(null, resultFor2.msg)
        assertEquals("eb6b05c2e3b008592ac666594d78ed83e7b9ab30f825b9b08878128f7500008c", resultFor2.needsToString())
        assertEquals("39b916432333e069a4386917609215cc688eb99f06fed01aadc29b1b4b92d6f0", resultFor2.sendsToString())
    }
}