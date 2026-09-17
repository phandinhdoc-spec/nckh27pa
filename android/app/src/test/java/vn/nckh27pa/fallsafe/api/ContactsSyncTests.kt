package vn.nckh27pa.fallsafe.api

import kotlinx.coroutines.runBlocking
import okhttp3.mockwebserver.*
import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.*

class ContactsSyncTests {
    @Test fun freshContactsAreEmptyAndLocalValidationWorksOffline() {
        val controller = DemoController(InMemoryContactRepository())
        val vm = ContactsViewModel(controller)
        assertTrue(controller.contacts.isEmpty())
        assertFalse(vm.add("", "", "0901234567", true, false))
        assertFalse(vm.add("Name", "", "123", true, false))
        assertTrue(vm.add("Name", "Family", "090 123 4567", true, false))
        val first = controller.contacts.single()
        assertTrue(first.isPrimary)
        assertFalse(vm.delete(first.id))
        assertTrue(vm.add("Second", "", "0912345678", true, false))
        vm.primary(controller.contacts.last().id)
        assertEquals(1, controller.contacts.count { it.isPrimary })
        vm.toggle(first.id)
        assertFalse(controller.contacts.first().receiveSos)
        assertTrue(vm.update(first.copy(name = "Updated", isPrimary = true)))
        assertTrue(vm.delete(controller.contacts.last().id))
        assertEquals("Updated", controller.contacts.single().name)
    }
    @Test fun crudUsesUserScopedPathsAndMapsValidationConflict() = runBlocking {
        val server = MockWebServer(); server.start()
        val client = ApiClient(ApiConfig(server.url("/").toString(), "phone", "alice"))
        val repo = ApiRepository(client.service)
        val c = EmergencyContact(name = "Person", relationship = "Family", phone = "0901234567", isPrimary = true)
        val json = com.google.gson.Gson().toJson(ContactDto.from(c, "alice"))
        try {
            server.enqueue(MockResponse().setBody("""{"success":true,"data":{"contact":$json},"timestamp":1}"""))
            assertTrue(repo.addContact("alice", c) is ApiResult.Success)
            assertEquals("/users/alice/contacts", server.takeRequest().path)
            server.enqueue(MockResponse().setBody("""{"success":true,"data":{"contact":$json},"timestamp":1}"""))
            assertTrue(repo.updateContact("alice", c) is ApiResult.Success)
            val put = server.takeRequest(); assertEquals("PUT", put.method); assertEquals("/users/alice/contacts/${c.id}", put.path)
            server.enqueue(MockResponse().setBody("""{"success":true,"data":{"deletedId":"${c.id}","remainingCount":1},"timestamp":1}"""))
            assertTrue(repo.deleteContact("alice", c.id) is ApiResult.Success)
            assertEquals("DELETE", server.takeRequest().method)
            for ((status, kind) in listOf(400 to ErrorKind.VALIDATION, 409 to ErrorKind.CONFLICT)) {
                server.enqueue(MockResponse().setResponseCode(status).setBody("""{"success":false,"error":{"code":"RULE","message":"Invalid"},"timestamp":1}"""))
                assertEquals(kind, (repo.addContact("alice", c) as ApiResult.Failure).kind)
            }
            server.enqueue(MockResponse().setBody("""{"success":true,"data":{"userId":"bob","contacts":[$json]},"timestamp":1}"""))
            assertEquals(ErrorKind.MALFORMED, (repo.contacts("alice") as ApiResult.Failure).kind)
        } finally { client.close(); server.shutdown() }
    }
}
