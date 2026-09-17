package vn.nckh27pa.fallsafe

import core.*
import org.junit.Assert.*
import org.junit.Test

class DemoTests {
    @Test fun pressureAltitudeUsesRelativeBaselineAndExpectedSign() {
        val n = pressureNormalizer(0, 1013.25f)
        assertEquals(0f, n.packet(0, 0)!!.altitudeDeltaM!!, 0.001f)

        n.update(SensorKind.PRESSURE, 100_000_000, floatArrayOf(1012.05f))
        val risen = n.packet(100_000_000, 0)!!
        assertEquals(101205f, risen.pressurePa!!, 0.1f)
        assertTrue(risen.altitudeDeltaM!! > 9f)
        assertTrue(risen.altitudeDeltaM!! < 11f)

        n.clear()
        n.update(SensorKind.ACCEL, 200_000_000, floatArrayOf(0f, 0f, 9.81f))
        n.update(SensorKind.PRESSURE, 200_000_000, floatArrayOf(1013.25f))
        n.update(SensorKind.PRESSURE, 300_000_000, floatArrayOf(1014.45f))
        val descended = n.packet(300_000_000, 0)!!.altitudeDeltaM!!
        assertTrue(descended < -9f)
        assertTrue(descended > -11f)
    }

    @Test fun pressureAltitudeIsNullWhenMissingStaleOrInvalid() {
        val n = PhoneNormalizer()
        n.update(SensorKind.ACCEL, 0, floatArrayOf(0f, 0f, 9.81f))
        assertNull(n.packet(0, 0)!!.altitudeDeltaM)
        n.update(SensorKind.PRESSURE, 0, floatArrayOf(1013.25f))
        assertNull(n.packet(500_000_001, 0))

        n.update(SensorKind.ACCEL, 600_000_000, floatArrayOf(0f, 0f, 9.81f))
        assertNull(n.packet(600_000_000, 0)!!.altitudeDeltaM)
        for (invalid in listOf(0f, -1f, Float.NaN, Float.POSITIVE_INFINITY)) {
            n.update(SensorKind.PRESSURE, 700_000_000, floatArrayOf(invalid))
            assertNull(n.packet(700_000_000, 0)!!.pressurePa)
            assertNull(n.packet(700_000_000, 0)!!.altitudeDeltaM)
        }
    }

    @Test fun pressureAltitudeResetTimestampSafetyAndBoundedWindow() {
        val n = pressureNormalizer(0, 1013.25f)
        n.update(SensorKind.PRESSURE, 100_000_000, floatArrayOf(1012.05f))
        assertTrue(n.packet(100_000_000, 0)!!.altitudeDeltaM!! > 9f)
        n.clear()
        n.update(SensorKind.ACCEL, 200_000_000, floatArrayOf(0f, 0f, 9.81f))
        n.update(SensorKind.PRESSURE, 200_000_000, floatArrayOf(1012.05f))
        assertEquals(0f, n.packet(200_000_000, 0)!!.altitudeDeltaM!!, 0.001f)

        n.update(SensorKind.PRESSURE, 100_000_000, floatArrayOf(900f))
        assertEquals(101205f, n.packet(200_000_000, 0)!!.pressurePa!!, 0.1f)
        assertEquals(0f, n.packet(200_000_000, 0)!!.altitudeDeltaM!!, 0.001f)

        repeat(200) { index ->
            val ns = 300_000_000L + index * 100_000_000L
            n.update(SensorKind.ACCEL, ns, floatArrayOf(0f, 0f, 9.81f))
            n.update(SensorKind.PRESSURE, ns, floatArrayOf(1013.25f))
        }
        val lastNs = 20_200_000_000L
        assertEquals(0f, n.packet(lastNs, 0)!!.altitudeDeltaM!!, 0.001f)
    }

    @Test fun normalizationAndMissingValues() {
        val n = PhoneNormalizer()
        assertNull(n.packet(0, 0))
        n.update(SensorKind.ACCEL, 0, floatArrayOf(0f, 0f, 9.81f))
        var p = n.packet(0, 123)!!
        assertNull(p.gyroXDps)
        assertEquals("UNKNOWN", p.phoneMotionState)
        assertEquals(0, p.phonePlacementConfidence)
        assertEquals(123L, p.wallClockTimestampMs)
        n.update(SensorKind.GYRO, 0, floatArrayOf(Math.PI.toFloat(), 0f, 0f))
        n.update(SensorKind.PRESSURE, 0, floatArrayOf(1013.25f))
        p = n.packet(0, 0)!!
        assertEquals(180f, p.gyroXDps!!, 0.001f)
        assertEquals(101325f, p.pressurePa!!, 0.1f)
        assertNull(p.stepCount)
        assertEquals(0f, p.altitudeDeltaM!!, 0.001f)
        n.update(SensorKind.ACCEL, 600_000_000, floatArrayOf(0f, 0f, 9.81f))
        assertNull(n.packet(600_000_000, 0)!!.pressurePa)
        assertNull(n.packet(1_200_000_000, 0))
    }
    @Test fun invalidAndFutureSamplesNeverBecomeMeasurements() {
        val n = PhoneNormalizer()
        n.update(SensorKind.ACCEL, 10, floatArrayOf(Float.NaN, 0f, 0f))
        assertNull(n.packet(10, 0))
        n.update(SensorKind.ACCEL, 20, floatArrayOf(0f, 0f, 9.81f))
        assertNull(n.packet(19, 0))
        n.update(SensorKind.GYRO, 20, floatArrayOf(Float.POSITIVE_INFINITY, 0f, 0f))
        assertNull(n.packet(20, 0)!!.gyroXDps)
        n.update(SensorKind.ACCEL, 21, floatArrayOf(1f))
        assertNull(n.packet(21, 0))
    }
    @Test fun detectorNeedsContinuousTimedEvidence() {
        val d = DemoDetector()
        assertFalse(d.accept(sample(0, 30f)))
        assertFalse(d.accept(sample(100, 9.81f)))
        assertFalse(d.accept(sample(2000, 9.81f))) // gap cannot prove quiet
        assertFalse(d.accept(sample(2100, 9.81f)))
        d.reset()
        assertFalse(d.accept(sample(0, 30f)))
        assertFalse(d.accept(sample(100, 9.81f)))
        assertFalse(d.accept(sample(100, 9.81f))) // duplicate resets
        assertFalse(d.accept(sample(200, 9.81f)))
        d.reset()
        repeat(30) { assertFalse(d.accept(sample(it * 100L, 9.81f))) }
        assertFalse(d.accept(sample(4000, Float.NaN)))
    }
    @Test fun replayUsesSamePipelineAndDeadline() {
        var now = 0L
        val sink = LocalDemoSink()
        val session = DemoSession(MonotonicClock { now }, sink)
        val replay = DemoReplay(0)
        assertTrue(replay.due(0).isNotEmpty())
        // Fresh replay: consume each sample exactly once at its scheduled time.
        val r = DemoReplay(0)
        for (t in 0L..1600L step 100) {
            now = t
            r.due(t).forEach { session.accept(it) }
        }
        assertTrue(r.finished)
        assertTrue(r.due(5000).isEmpty())
        assertEquals(State.VERIFYING, session.snapshot().state)
        val remaining = session.snapshot().remainingMs!!
        now += remaining - 1
        session.tick()
        assertEquals(Status.COUNTDOWN, session.snapshot().status)
        now++
        session.tick()
        assertEquals(Response.NO_RESPONSE, session.snapshot().response)
        assertEquals(Status.SENT, session.snapshot().status)
        assertEquals(1, sink.alerts().size)
        session.tick()
        assertEquals(1, sink.alerts().size)
    }
    @Test fun safeFailureCompleteAndBoundedHistory() {
        var now = 0L
        val sink = LocalDemoSink()
        val s = DemoSession(MonotonicClock { now }, sink)
        DemoReplay(0).due(1600).forEach { s.accept(it) }
        s.safe()
        now = 20000
        s.tick()
        assertTrue(sink.alerts().isEmpty())
        sink.fail = true
        s.needHelp()
        assertEquals(Status.FAILED, s.snapshot().status)
        s.complete()
        sink.fail = false
        repeat(40) { s.needHelp(); s.complete() }
        assertEquals(32, sink.alerts().size)
        assertTrue(s.events().size <= 32)
        assertEquals(Status.ACKNOWLEDGED, s.snapshot().status)
    }
    @Test fun sosRequiresTwoSecondsAndCancels() {
        val h = SosHold()
        h.start(100)
        assertFalse(h.ready(2099))
        assertTrue(h.ready(2100))
        assertFalse(h.ready(2200))
        h.start(3000)
        h.cancel()
        assertFalse(h.ready(6000))
    }
    @Test fun optionalNormalizationAndFreshnessBoundary() {
        val n = PhoneNormalizer()
        n.update(SensorKind.ACCEL, 0, floatArrayOf(1f, 2f, 3f))
        n.update(SensorKind.LINEAR, 0, floatArrayOf(4f, 5f, 6f))
        n.update(SensorKind.ORIENTATION, 0, floatArrayOf(10f, 20f, 30f))
        val p = n.packet(500_000_000, 0)!!
        assertEquals(6f, p.linearAccelZMs2!!, 0f)
        assertEquals(10f, p.pitchDeg!!, 0f)
        assertEquals(30f, p.yawDeg!!, 0f)
        assertNull(n.packet(500_000_001, 0))
        n.clear()
        assertNull(n.packet(0, 0))
    }
    @Test fun motionAndExpiredWindowCannotConfirm() {
        val d = DemoDetector()
        assertFalse(d.accept(sample(0, 30f)))
        for (t in 100L..4000L step 100) {
            assertFalse(d.accept(sample(t, if (t < 3000) 15f else 9.81f)))
        }
        d.reset()
        assertFalse(d.accept(sample(0, 30f)))
        for (t in 100L..1000L step 100) assertFalse(d.accept(sample(t, 9.81f)))
        assertFalse(d.accept(sample(1050, Float.NaN)))
        for (t in 1100L..2200L step 100) assertFalse(d.accept(sample(t, 9.81f)))
    }
    @Test fun helpDuringCountdownAndSafeAtDeadline() {
        var now = 0L
        val s = DemoSession(MonotonicClock { now })
        DemoReplay(0).due(1600).forEach(s::accept)
        s.needHelp()
        assertEquals(Response.NEED_HELP, s.snapshot().response)
        assertEquals(1, s.sink.alerts().size)
        s.complete()
        DemoReplay(2000).due(3600).forEach(s::accept)
        now = 10000
        s.safe()
        assertEquals(Response.NO_RESPONSE, s.snapshot().response)
        assertEquals(Status.SENT, s.snapshot().status)
        assertEquals(2, s.sink.alerts().size)
    }
    private class PipelineFixture {
        var now = 0L
        var phoneOnly = true
        var displayed: PhoneSensorPacket? = null
        val session = DemoSession(MonotonicClock { now })
        val adapter = DemoInputAdapter(session, { phoneOnly }, { displayed = it })
        val pipeline = PhoneInputPipeline(adapter::acceptPhone)
        fun sample(ms: Long, z: Float, kind: SensorKind = SensorKind.ACCEL) {
            now = ms
            pipeline.update(kind, ms * 1_000_000, floatArrayOf(0f, 0f, z), ms * 1_000_000, ms)
        }
        fun pause() { pipeline.clear(); adapter.paused() }
        fun quiet() { for (t in 100L..1000L step 100) sample(t, 9.81f) }
    }
    @Test fun replayImpactSurvivesActivityPauseAndSendsAtDeadline() {
        val f = PipelineFixture()
        f.phoneOnly = false
        val replay = DemoReplay(0)
        for (t in 0L..1600L step 100) {
            f.now = t
            replay.due(t).forEach(f.adapter::acceptReplay)
            if (t == 500L) f.pause()
        }
        assertTrue(replay.finished)
        assertEquals(State.VERIFYING, f.session.snapshot().state)
        val remaining = f.session.snapshot().remainingMs!!
        f.pause()
        assertEquals(remaining, f.session.snapshot().remainingMs)
        f.now += remaining
        f.session.tick()
        assertEquals(Status.SENT, f.session.snapshot().status)
        assertEquals(1, f.session.sink.alerts().size)
    }
    @Test fun invalidAccelThroughNormalizerBreaksQuietWindow() {
        val f = PipelineFixture()
        f.sample(0, 30f)
        f.quiet()
        f.sample(1050, Float.NaN)
        assertNull(f.pipeline.latest(1_050_000_000, 1050))
        for (t in 1100L..2200L step 100) f.sample(t, 9.81f)
        assertEquals(State.MONITORING, f.session.snapshot().state)
        assertNull(f.session.snapshot().remainingMs)
        assertTrue(f.session.sink.alerts().isEmpty())
        // Recovery still accepts a fresh, complete sequence.
        f.sample(2300, 30f)
        for (t in 2400L..3400L step 100) f.sample(t, 9.81f)
        assertEquals(State.VERIFYING, f.session.snapshot().state)
    }
    @Test fun invalidOptionalSensorsDoNotBreakAccelEvidence() {
        for (kind in SensorKind.values().filter { it != SensorKind.ACCEL }) {
            val f = PipelineFixture()
            f.sample(0, 30f)
            f.quiet()
            f.sample(1050, Float.NaN, kind)
            assertNotNull(f.pipeline.latest(1_050_000_000, 1050))
            f.sample(1100, 9.81f)
            assertEquals(kind.toString(), State.VERIFYING, f.session.snapshot().state)
        }
    }
    @Test fun phonePauseClearsEvidenceAndDisplayButPreservesDeadline() {
        val f = PipelineFixture()
        f.sample(0, 30f)
        f.quiet()
        f.pause()
        assertNull(f.displayed)
        assertNull(f.pipeline.latest(1_000_000_000, 1000))
        f.sample(1100, 9.81f)
        assertEquals(State.MONITORING, f.session.snapshot().state)
        f.sample(1200, 30f)
        for (t in 1300L..2300L step 100) f.sample(t, 9.81f)
        val remaining = f.session.snapshot().remainingMs!!
        f.pause()
        assertEquals(remaining, f.session.snapshot().remainingMs)
        f.now += remaining
        f.session.tick()
        assertEquals(Status.SENT, f.session.snapshot().status)
    }
    @Test fun pauseWithServiceOwnershipKeepsPhoneEvidence() {
        val f = PipelineFixture()
        f.sample(0, 30f); f.quiet()
        f.adapter.paused(backgroundMonitoring = true)
        f.sample(1100, 9.81f)
        assertEquals(State.VERIFYING, f.session.snapshot().state)
    }
    private fun sample(ms: Long, z: Float) = PhoneSensorPacket(
        timestampNs = ms * 1_000_000, wallClockTimestampMs = 0,
        accelXMs2 = 0f, accelYMs2 = 0f, accelZMs2 = z
    )

    private fun pressureNormalizer(ns: Long, pressureHpa: Float) = PhoneNormalizer().also {
        it.update(SensorKind.ACCEL, ns, floatArrayOf(0f, 0f, 9.81f))
        it.update(SensorKind.PRESSURE, ns, floatArrayOf(pressureHpa))
    }
}
