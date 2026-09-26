package vn.nckh27pa.fallsafe

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SettingsSensorDiagnosticsTest {

    private fun sampleObservation(phase: DetectionPhase = DetectionPhase.NORMAL) = FallDetectionObservation(
        accelerationMagnitudeMs2 = null,
        phase = phase,
        activeProfileId = "p1",
        activeProfileDisplayName = "Bảng 1",
        config = FallDetectionConfig.DEFAULT,
        impactOverThreshold = false,
        withinStillness = false,
        stillnessProgress = 0f,
        stillnessSampleCount = 0
    )

    @Test
    fun `case 1 - still phone accel only produces live state phone source and valid magnitude with null gyro`() {
        val packet = PhoneSensorPacket(
            timestampNs = 1_000_000L,
            wallClockTimestampMs = 1_000L,
            accelXMs2 = 0f,
            accelYMs2 = 0f,
            accelZMs2 = 9.80665f
        )
        val diagnostics = settingsSensorDiagnostics(packet, sampleObservation(), sensorsAvailable = true)

        assertEquals(SensorDataState.LIVE, diagnostics.dataState)
        assertEquals("PHONE", diagnostics.source)
        assertEquals(0f, diagnostics.accelX)
        assertEquals(0f, diagnostics.accelY)
        assertEquals(9.80665f, diagnostics.accelZ)
        assertEquals(9.80665, diagnostics.accelMagnitudeMs2!!, 0.01)
        assertNull(diagnostics.gyroXRadS)
        assertNull(diagnostics.gyroYRadS)
        assertNull(diagnostics.gyroZRadS)
        assertNull(diagnostics.gyroMagnitudeRadS)
    }

    @Test
    fun `case 2 - gyro converted from dps to rad per s for display only`() {
        val packet = PhoneSensorPacket(
            timestampNs = 1_000_000L,
            wallClockTimestampMs = 1_000L,
            accelXMs2 = 0f,
            accelYMs2 = 9.8f,
            accelZMs2 = 0f,
            gyroXDps = 180f,
            gyroYDps = -90f,
            gyroZDps = 0f
        )
        val diagnostics = settingsSensorDiagnostics(packet, sampleObservation(), sensorsAvailable = true)

        assertEquals(3.141593f, diagnostics.gyroXRadS!!, 1e-4f)
        assertEquals(-1.570796f, diagnostics.gyroYRadS!!, 1e-4f)
        assertEquals(0.0f, diagnostics.gyroZRadS!!, 1e-4f)
        assertEquals(3.512407, diagnostics.gyroMagnitudeRadS!!, 1e-4)
    }

    @Test
    fun `case 3 - dataState reflects packet presence and sensor availability`() {
        val packet = PhoneSensorPacket(
            timestampNs = 1_000_000L,
            wallClockTimestampMs = 1_000L,
            accelXMs2 = 0f,
            accelYMs2 = 9.8f,
            accelZMs2 = 0f
        )

        // packet = null, sensorsAvailable = true -> LOST
        val lost = settingsSensorDiagnostics(null, sampleObservation(), sensorsAvailable = true)
        assertEquals(SensorDataState.LOST, lost.dataState)

        // packet = null, sensorsAvailable = false -> UNSUPPORTED
        val unsupported = settingsSensorDiagnostics(null, sampleObservation(), sensorsAvailable = false)
        assertEquals(SensorDataState.UNSUPPORTED, unsupported.dataState)

        // packet present -> LIVE even when sensorsAvailable = false
        val live = settingsSensorDiagnostics(packet, sampleObservation(), sensorsAvailable = false)
        assertEquals(SensorDataState.LIVE, live.dataState)
    }

    @Test
    fun `case 4 - fallPhase mirrors observation phase`() {
        val observation = sampleObservation(phase = DetectionPhase.IMPACT_DETECTED)
        val diagnostics = settingsSensorDiagnostics(null, observation, sensorsAvailable = true)
        assertEquals(DetectionPhase.IMPACT_DETECTED, diagnostics.fallPhase)
    }

    @Test
    fun `case 5 - formatting numbers to fixed decimals and handling null or nan`() {
        assertEquals("9.81", formatDiagnosticNumber(9.80665, 2))
        assertEquals("0.087", formatDiagnosticNumber(0.0865f, 3))
        assertEquals("—", formatDiagnosticNumber(null as Double?, 2))
        assertEquals("—", formatDiagnosticNumber(Float.NaN, 3))
    }

    @Test
    fun `case 6 - sensorDataStateVietnamese returns expected labels`() {
        assertEquals("Đang nhận dữ liệu", sensorDataStateVietnamese(SensorDataState.LIVE))
        assertEquals("Mất dữ liệu", sensorDataStateVietnamese(SensorDataState.LOST))
        assertEquals("Không hỗ trợ", sensorDataStateVietnamese(SensorDataState.UNSUPPORTED))
    }
}
