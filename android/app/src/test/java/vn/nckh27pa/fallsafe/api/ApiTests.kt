package vn.nckh27pa.fallsafe.api

import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.BuildConfig

class ApiTests {
    @Test fun configurationNormalizesAndUsesGeneratedValues() {
        assertEquals("http://localhost:3000/api/v1/", ApiConfig.normalize(" http://localhost:3000/api/v1/// "))
        val c = ApiConfig.generated()
        assertEquals(BuildConfig.API_BASE_URL, c.baseUrl)
        assertEquals(BuildConfig.FALLSAFE_DEVICE_ID, c.deviceId)
        assertEquals(BuildConfig.FALLSAFE_USER_ID, c.userId)
    }
    @Test fun envelopesErrorsAndMalformedResponses() = runBlocking {
        val server = MockWebServer(); server.start()
        val client = ApiClient(ApiConfig(server.url("/").toString(), "phone", "user"))
        try {
            val repo = ApiRepository(client.service)
            server.enqueue(MockResponse().setBody("""{"success":true,"data":{"userId":"user","contacts":[]},"timestamp":1}"""))
            assertTrue(repo.contacts("user") is ApiResult.Success)
            val req = server.takeRequest()
            assertEquals("/users/user/contacts", req.path)
            assertEquals("phone", req.getHeader("X-Device-Id"))
            server.enqueue(MockResponse().setResponseCode(409).setBody("""{"success":false,"error":{"code":"CANNOT_DELETE_LAST_CONTACT","message":"last"},"timestamp":1}"""))
            assertEquals(ErrorKind.CONFLICT, (repo.contacts("user") as ApiResult.Failure).kind)
            server.enqueue(MockResponse().setBody("not json"))
            assertEquals(ErrorKind.MALFORMED, (repo.contacts("user") as ApiResult.Failure).kind)
            server.enqueue(MockResponse().setBody("""{"success":true,"data":{},"timestamp":1}"""))
            assertEquals(ErrorKind.MALFORMED, (repo.contacts("user") as ApiResult.Failure).kind)
        } finally { client.close(); server.shutdown() }
    }
    @Test fun refusedConnectionAndTimeoutAreValues() = runBlocking {
        val server = MockWebServer(); server.start()
        val config = ApiConfig(server.url("/").toString(), "phone", "user")
        val client = ApiClient(config, 100)
        try {
            server.enqueue(MockResponse().setSocketPolicy(okhttp3.mockwebserver.SocketPolicy.NO_RESPONSE))
            assertEquals(ErrorKind.TIMEOUT, (ApiRepository(client.service).contacts("user") as ApiResult.Failure).kind)
            server.shutdown()
            assertEquals(ErrorKind.OFFLINE, (ApiRepository(client.service).contacts("user") as ApiResult.Failure).kind)
        } finally { client.close() }
    }
}
