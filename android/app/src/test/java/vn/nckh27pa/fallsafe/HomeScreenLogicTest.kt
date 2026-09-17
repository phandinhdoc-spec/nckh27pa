package vn.nckh27pa.fallsafe

import core.*
import org.junit.Assert.*
import org.junit.Test

class HomeScreenLogicTest {

    @Test
    fun testSosHoldRequiresThreeSecondsAndDoesNotTriggerOnQuickTap() {
        val sosHold = SosHold(3000L)
        val startTime = 1000L

        // User starts touching the button
        sosHold.start(startTime)

        // 1. Quick tap (e.g. 150 ms) -> must NOT trigger
        assertFalse("Quick tap (150ms) must not trigger SOS", sosHold.ready(startTime + 150))

        // 2. Holding for 1 second -> must NOT trigger
        assertFalse("1000ms must not trigger 3-second SOS", sosHold.ready(startTime + 1000))

        // 3. Holding for 2 seconds -> must NOT trigger
        assertFalse("2000ms must not trigger 3-second SOS", sosHold.ready(startTime + 2000))

        // 4. Holding for 2999 ms -> must NOT trigger
        assertFalse("2999ms must not trigger 3-second SOS", sosHold.ready(startTime + 2999))

        // 5. Holding for exactly 3000 ms -> MUST trigger
        assertTrue("3000ms must trigger SOS", sosHold.ready(startTime + 3000))

        // 6. After triggering, it resets
        assertFalse("After triggering, ready must return false", sosHold.ready(startTime + 3001))

        // 7. Releasing early (cancel) resets the hold
        sosHold.start(5000L)
        assertFalse(sosHold.ready(5000L + 2500L))
        sosHold.cancel()
        assertFalse("Cancelled hold must not trigger even after elapsed time", sosHold.ready(5000L + 4000L))
    }

    @Test
    fun testCancelHoldRequiresTwoSeconds() {
        val cancelHold = SosHold(2000L)
        val startTime = 1000L

        // User starts touching "Tôi vẫn ổn" button to cancel
        cancelHold.start(startTime)

        // 1. Quick tap (200ms) -> must NOT cancel
        assertFalse("Quick tap (200ms) must not cancel warning", cancelHold.ready(startTime + 200))

        // 2. Holding for 1000 ms -> must NOT cancel
        assertFalse("1000ms must not cancel 2-second hold", cancelHold.ready(startTime + 1000))

        // 3. Holding for 1999 ms -> must NOT cancel
        assertFalse("1999ms must not cancel 2-second hold", cancelHold.ready(startTime + 1999))

        // 4. Holding for exactly 2000 ms -> MUST cancel
        assertTrue("2000ms must trigger cancel", cancelHold.ready(startTime + 2000))

        // 5. Releasing before 2 seconds resets
        cancelHold.start(10000L)
        cancelHold.cancel()
        assertFalse("Cancelled hold must not trigger", cancelHold.ready(10000L + 3000L))
    }

    @Test
    fun testCountdownExpiryAutomaticallySendsSosWithoutUserInput() {
        var currentTimeMs = 0L
        val sink = LocalDemoSink()
        val session = DemoSession(MonotonicClock { currentTimeMs }, sink)

        // Trigger fall evidence -> VERIFYING (10s countdown)
        DemoReplay(0).due(1600).forEach { session.accept(it) }
        assertEquals("Should be in VERIFYING state", State.VERIFYING, session.snapshot().state)
        assertEquals("Status should be COUNTDOWN", Status.COUNTDOWN, session.snapshot().status)
        assertTrue("No alert sent yet", sink.alerts().isEmpty())

        // 9.9 seconds elapsed without user action
        currentTimeMs = 9900L
        session.tick()
        assertEquals("Should still be in COUNTDOWN", Status.COUNTDOWN, session.snapshot().status)
        assertTrue("Sink must still have 0 alerts", sink.alerts().isEmpty())

        // 10.0 seconds elapsed (timeout) -> system must automatically dispatch SOS!
        currentTimeMs = 10000L
        session.tick()

        assertEquals("State must transition to AWAITING_HELP", State.AWAITING_HELP, session.snapshot().state)
        assertEquals("Status must be SENT", Status.SENT, session.snapshot().status)
        assertEquals("Response must be NO_RESPONSE", Response.NO_RESPONSE, session.snapshot().response)
        assertEquals("Sink must have received exactly 1 alert", 1, sink.alerts().size)
    }

    @Test
    fun testAllFiveMainScreenStatuses() {
        // 1. Bình thường: SAFE
        assertEquals(
            MainScreenStatus.SAFE,
            resolveMainScreenStatus(deviceConnected = true, state = State.MONITORING, status = Status.NOT_REQUIRED, caregiverAcknowledged = false)
        )

        // 2. Đang đếm ngược cảnh báo: WARNING_COUNTDOWN
        assertEquals(
            MainScreenStatus.WARNING_COUNTDOWN,
            resolveMainScreenStatus(deviceConnected = true, state = State.VERIFYING, status = Status.COUNTDOWN, caregiverAcknowledged = false)
        )

        // 3. Đã gửi SOS: SOS_SENT
        assertEquals(
            MainScreenStatus.SOS_SENT,
            resolveMainScreenStatus(deviceConnected = true, state = State.ALERTING, status = Status.SENDING, caregiverAcknowledged = false)
        )
        assertEquals(
            MainScreenStatus.SOS_SENT,
            resolveMainScreenStatus(deviceConnected = true, state = State.AWAITING_HELP, status = Status.SENT, caregiverAcknowledged = false)
        )

        // 4. Người thân đã nhận tin: HELP_ACKNOWLEDGED
        assertEquals(
            MainScreenStatus.HELP_ACKNOWLEDGED,
            resolveMainScreenStatus(deviceConnected = true, state = State.AWAITING_HELP, status = Status.SENT, caregiverAcknowledged = true)
        )
        assertEquals(
            MainScreenStatus.HELP_ACKNOWLEDGED,
            resolveMainScreenStatus(deviceConnected = true, state = State.MONITORING, status = Status.ACKNOWLEDGED, caregiverAcknowledged = false)
        )

        // 5. Mất kết nối thiết bị khi đang bình thường: DEVICE_DISCONNECTED
        assertEquals(
            MainScreenStatus.DEVICE_DISCONNECTED,
            resolveMainScreenStatus(deviceConnected = false, state = State.MONITORING, status = Status.NOT_REQUIRED, caregiverAcknowledged = false)
        )
    }

    @Test
    fun testEmergencyPriorityOverDeviceDisconnection() {
        // Safety Requirement: When device disconnects during emergencies, the center button
        // must NOT be replaced by DEVICE_DISCONNECTED; emergency operations must remain accessible!

        // A. Disconnection during countdown (VERIFYING) -> must remain WARNING_COUNTDOWN (allow cancel)
        assertEquals(
            "Disconnection during countdown must keep WARNING_COUNTDOWN",
            MainScreenStatus.WARNING_COUNTDOWN,
            resolveMainScreenStatus(deviceConnected = false, state = State.VERIFYING, status = Status.COUNTDOWN, caregiverAcknowledged = false)
        )

        // B. Disconnection during SUSPECTED -> must remain WARNING_COUNTDOWN
        assertEquals(
            "Disconnection during suspected state must keep WARNING_COUNTDOWN",
            MainScreenStatus.WARNING_COUNTDOWN,
            resolveMainScreenStatus(deviceConnected = false, state = State.SUSPECTED, status = Status.NOT_REQUIRED, caregiverAcknowledged = false)
        )

        // C. Disconnection during ALERTING -> must remain SOS_SENT
        assertEquals(
            "Disconnection during alerting must keep SOS_SENT",
            MainScreenStatus.SOS_SENT,
            resolveMainScreenStatus(deviceConnected = false, state = State.ALERTING, status = Status.SENDING, caregiverAcknowledged = false)
        )

        // D. Disconnection during AWAITING_HELP -> must remain SOS_SENT
        assertEquals(
            "Disconnection during awaiting help must keep SOS_SENT",
            MainScreenStatus.SOS_SENT,
            resolveMainScreenStatus(deviceConnected = false, state = State.AWAITING_HELP, status = Status.SENT, caregiverAcknowledged = false)
        )

        // E. Disconnection after caregiver acknowledged -> must remain HELP_ACKNOWLEDGED
        assertEquals(
            "Disconnection when caregiver acknowledged must keep HELP_ACKNOWLEDGED",
            MainScreenStatus.HELP_ACKNOWLEDGED,
            resolveMainScreenStatus(deviceConnected = false, state = State.AWAITING_HELP, status = Status.SENT, caregiverAcknowledged = true)
        )
        assertEquals(
            "Disconnection with completed acknowledged event must keep HELP_ACKNOWLEDGED",
            MainScreenStatus.HELP_ACKNOWLEDGED,
            resolveMainScreenStatus(deviceConnected = false, state = State.MONITORING, status = Status.ACKNOWLEDGED, caregiverAcknowledged = false)
        )

        // F. Disconnection ONLY shows as main status during normal MONITORING
        assertEquals(
            "Disconnection during normal monitoring must show DEVICE_DISCONNECTED",
            MainScreenStatus.DEVICE_DISCONNECTED,
            resolveMainScreenStatus(deviceConnected = false, state = State.MONITORING, status = Status.NOT_REQUIRED, caregiverAcknowledged = false)
        )
    }

    @Test
    fun testSosHoldProgressCalculation() {
        val hold = SosHold(3000L)
        assertEquals(0f, hold.progress(1000L), 0.001f)

        hold.start(1000L)
        assertEquals(0f, hold.progress(1000L), 0.001f)
        assertEquals(0.5f, hold.progress(2500L), 0.01f)
        assertEquals(1.0f, hold.progress(4000L), 0.001f)
        assertEquals(1.0f, hold.progress(5000L), 0.001f)
    }

    @Test
    fun testPhoneNormalizationAndValidation() {
        // Normalization removes spaces, dashes, dots, parentheses
        assertEquals("0901234567", ContactValidator.normalize(" 090 123 4567 "))
        assertEquals("0901234567", ContactValidator.normalize("090-123-4567"))
        assertEquals("0901234567", ContactValidator.normalize("(090) 123.4567"))
        assertEquals("+84901234567", ContactValidator.normalize("+84 901-234-567"))

        // Valid VN phone numbers
        assertNull(ContactValidator.validate("0901234567"))
        assertNull(ContactValidator.validate("0381234567"))
        assertNull(ContactValidator.validate("0771234567"))
        assertNull(ContactValidator.validate("0912345678"))
        assertNull(ContactValidator.validate("+84901234567"))
        assertNull(ContactValidator.validate("+84381234567"))

        // Invalid phone numbers
        assertNotNull(ContactValidator.validate(""))
        assertNotNull(ContactValidator.validate("   "))
        assertNotNull(ContactValidator.validate("012345678")) // too short (9 digits)
        assertNotNull(ContactValidator.validate("090123456789")) // too long (12 digits)
        assertNotNull(ContactValidator.validate("0201234567")) // invalid mobile prefix 02
        assertNotNull(ContactValidator.validate("abcdefghij")) // non-numeric
        assertNotNull(ContactValidator.validate("+1234567890")) // non-VN international prefix

        // Regression tests for Hermes finding 4: pipe character inside [35789] must be rejected
        assertNotNull("0|12345678 containing pipe character must be rejected", ContactValidator.validate("0|12345678"))
        assertNotNull("+84|12345678 containing pipe character must be rejected", ContactValidator.validate("+84|12345678"))
    }

    @Test
    fun testPhoneMasking() {
        assertEquals("090…567", ContactValidator.mask("0901234567"))
        assertEquals("091…678", ContactValidator.mask("0912345678"))
        assertEquals("+8490…567", ContactValidator.mask("+84901234567"))
        assertEquals("123", ContactValidator.mask("123")) // too short to mask
    }

    @Test
    fun testContactRepositoryAndCrud() {
        val repo = InMemoryContactRepository(emptyList())
        val controller = DemoController(repo)

        // 1. Initial defaults seeded
        val initialDefaults = listOf(
            EmergencyContact(name = "Nguyễn Thị Mai", relationship = "Con gái", phone = "0901234567", isPrimary = true),
            EmergencyContact(name = "Nguyễn Văn Tuấn", relationship = "Con trai", phone = "0912345678")
        )
        repo.saveContacts(initialDefaults)
        controller.reloadContactsFromRepo()

        assertEquals(2, controller.contacts.size)
        assertEquals("Con gái", controller.primaryContactName)
        assertEquals("Nguyễn Thị Mai", controller.primaryContactFullName)
        assertEquals("0901234567", controller.primaryContactPhone)

        // 2. Add new contact
        val addSuccess = controller.addContact(
            name = "Trần Thị Lan",
            relationship = "Cháu",
            phone = "0987654321",
            receiveSos = true,
            isPrimary = false
        )
        assertTrue("Add valid contact must succeed", addSuccess)
        assertEquals(3, controller.contacts.size)

        // 3. Add contact with invalid phone fails
        val failAdd = controller.addContact("Ai Đó", "Bạn", "12345", true, false)
        assertFalse("Add with invalid phone must fail", failAdd)
        assertEquals(3, controller.contacts.size)

        // 4. Update contact
        val toUpdate = controller.contacts.first { it.name == "Trần Thị Lan" }
        val updateSuccess = controller.updateContact(toUpdate.copy(relationship = "Người chăm sóc"))
        assertTrue("Update must succeed", updateSuccess)
        assertEquals("Người chăm sóc", controller.contacts.first { it.id == toUpdate.id }.relationship)

        // 5. Change primary contact
        controller.setPrimaryContact(toUpdate.id)
        assertEquals("Người chăm sóc", controller.primaryContactName)
        assertEquals("Trần Thị Lan", controller.primaryContactFullName)
        assertEquals(toUpdate.id, controller.primaryContact?.id)

        // 6. Toggle receive SOS
        val wasReceiving = controller.contacts.first { it.id == toUpdate.id }.receiveSos
        controller.toggleReceiveSos(toUpdate.id)
        assertNotEquals(wasReceiving, controller.contacts.first { it.id == toUpdate.id }.receiveSos)

        // 7. Delete contact
        val deleteSuccess = controller.deleteContact(toUpdate.id)
        assertTrue("Delete must succeed when size > 1", deleteSuccess)
        assertEquals(2, controller.contacts.size)
        // Since primary was deleted, a remaining contact automatically becomes primary
        assertNotNull("Must maintain a primary contact", controller.primaryContact)
        assertTrue("Remaining primary must have isPrimary = true", controller.primaryContact!!.isPrimary)

        // 8. Delete down to 1 contact
        val secondId = controller.contacts[1].id
        controller.deleteContact(secondId)
        assertEquals(1, controller.contacts.size)

        // 9. Rule: CANNOT delete the last contact without warning / system rejects
        val lastId = controller.contacts[0].id
        val deleteLastSuccess = controller.deleteContact(lastId)
        assertFalse("Must NOT allow deleting the last contact", deleteLastSuccess)
        assertEquals(1, controller.contacts.size)
    }

    @Test
    fun testHoldTimingEdgeCases() {
        val hold = SosHold(3000L)

        // Under threshold
        hold.start(100L)
        assertFalse(hold.ready(100L))
        assertFalse(hold.ready(1000L))
        assertFalse(hold.ready(3099L))

        // Exact threshold (100 + 3000 = 3100)
        assertTrue(hold.ready(3100L))

        // Ready resets internal state; subsequent calls return false (no duplicate trigger)
        assertFalse("Duplicate ready call must be false", hold.ready(3101L))
        assertFalse("Must not be holding after fire", hold.isHolding())

        // Re-starting after fire works cleanly
        hold.start(5000L)
        assertTrue(hold.isHolding())
        hold.cancel()
        assertFalse(hold.isHolding())
        assertFalse(hold.ready(8000L))
    }
}
