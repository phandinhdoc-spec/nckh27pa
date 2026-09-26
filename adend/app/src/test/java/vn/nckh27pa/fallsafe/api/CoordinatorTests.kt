package vn.nckh27pa.fallsafe.api

import core.MonotonicClock
import core.State
import kotlinx.coroutines.*
import okhttp3.mockwebserver.*
import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.*
import vn.nckh27pa.fallsafe.emergency.SmsDeliveryStatus
import vn.nckh27pa.fallsafe.emergency.SmsDispatchState

class CoordinatorTests {
    @Test fun terminalSmsCallbackIsDurablyQueuedForAuthenticatedBackendReport() {
        val store=MemorySyncStore();val outbox=SyncOutbox(store)
        val controller=DemoController()
        val server=MockWebServer();server.start()
        val client=ApiClient(ApiConfig(server.url("/").toString(),"PHONE-DEFAULT","user",null))
        val coordinator=SyncCoordinator(controller,ApiRepository(client.service),ApiConfig(server.url("/").toString(),"PHONE-DEFAULT","user",null),outbox,dispatcher=Dispatchers.Unconfined,wallMs={42})
        try {
            coordinator.reportTransportState(SmsDispatchState("event-1","contact-1",SmsDeliveryStatus.SENT,"network accepted"))
            val operation=outbox.pending().single()
            assertEquals(OperationKind.TRANSPORT_STATUS,operation.kind)
            assertEquals("event-1",operation.contactId)
            assertEquals("SENT",operation.transportStatus!!.status)
        } finally {coordinator.close();client.close();server.shutdown()}
    }
    @Test fun backendAckUpdatesExistingStateAndOfflineNeverStopsCountdown() = runBlocking {
        val server = MockWebServer(); server.start()
        val config = ApiConfig(server.url("/").toString(), "phone", "user")
        val client = ApiClient(config)
        var now = 0L
        val controller = DemoController(InMemoryContactRepository(), MonotonicClock { now })
        val outbox = SyncOutbox(MemorySyncStore())
        val coordinator = SyncCoordinator(controller, ApiRepository(client.service), config, outbox,
            dispatcher = Dispatchers.Unconfined, wallMs = { now })
        try {
            DemoReplay(0).due(1600).forEach(controller.session::accept)
            controller.refresh()
            assertEquals(State.VERIFYING, controller.snapshot.state)
            assertEquals(OperationKind.EVENT, outbox.pending().single().kind)
            server.dispatcher = object : Dispatcher() {
                override fun dispatch(request: RecordedRequest) = MockResponse().setResponseCode(503)
                    .setBody("""{"success":false,"error":{"code":"SERVICE_UNAVAILABLE","message":"offline"},"timestamp":1}""")
            }
            coordinator.syncOnce()
            assertEquals(State.VERIFYING, controller.snapshot.state)
            now = 10000; controller.session.tick(); controller.refresh()
            assertEquals(State.AWAITING_HELP, controller.snapshot.state)
            assertFalse(controller.caregiverAcknowledged)
            val id = coordinator.remoteEventId()
            server.dispatcher = object : Dispatcher() {
                override fun dispatch(request: RecordedRequest): MockResponse {
                    val data = when {
                        request.path!!.startsWith("/alerts/active") -> """{"activeEventId":"$id","alertState":"AWAITING_HELP","caregiverAcknowledged":true}"""
                        request.path!!.contains("contacts") -> """{"userId":"user","contacts":[]}"""
                        else -> """{"eventId":"$id","alertState":"AWAITING_HELP","dispatchStatus":"RECORDED"}"""
                    }
                    return MockResponse().setBody("""{"success":true,"data":$data,"timestamp":1}""")
                }
            }
            coordinator.syncOnce()
            assertTrue(controller.caregiverAcknowledged)
            assertEquals(MainScreenStatus.HELP_ACKNOWLEDGED, controller.mainScreenStatus)
            assertFalse(controller.sosDeliveryMessage.contains("đã gửi", ignoreCase = true))
            assertTrue(outbox.pending().isEmpty())
        } finally { coordinator.close(); client.close(); server.shutdown() }
    }
    @Test fun contactChangesPersistOfflineThenReconcileAndCloseDetaches() = runBlocking {
        val server = MockWebServer(); server.start()
        val config = ApiConfig(server.url("/").toString(), "phone", "user")
        val client = ApiClient(config)
        val store = MemorySyncStore(); val outbox = SyncOutbox(store)
        val controller = DemoController()
        val coordinator = SyncCoordinator(controller, ApiRepository(client.service), config, outbox, dispatcher = Dispatchers.Unconfined)
        val vm = ContactsViewModel(controller)
        vm.add("Name", "", "0901234567", true, false)
        assertEquals(OperationKind.CONTACT_ADD, SyncOutbox(store).pending().single().kind)
        val contact = controller.contacts.single()
        val dto = com.google.gson.Gson().toJson(ContactDto.from(contact.copy(name = "Server normalized"), "user"))
        server.dispatcher = object : Dispatcher() {
            override fun dispatch(request: RecordedRequest): MockResponse {
                val data = if (request.method == "POST") """{"contact":$dto}""" else """{"userId":"user","contacts":[$dto]}"""
                return MockResponse().setBody("""{"success":true,"data":$data,"timestamp":1}""")
            }
        }
        try {
            coordinator.syncOnce()
            assertEquals("Server normalized", controller.contacts.single().name)
            assertTrue(outbox.pending().isEmpty())
        } finally { coordinator.close(); client.close(); server.shutdown() }
        assertNull(controller.onContactMutation)
        assertNull(controller.onSyncStateChanged)
    }
}
