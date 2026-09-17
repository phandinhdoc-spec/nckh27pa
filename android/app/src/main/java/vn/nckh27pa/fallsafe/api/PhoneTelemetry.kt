package vn.nckh27pa.fallsafe.api

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.Sensor
import android.hardware.SensorManager
import android.os.BatteryManager
import android.os.SystemClock
import vn.nckh27pa.fallsafe.BuildConfig
import vn.nckh27pa.fallsafe.PhoneSensorPacket

data class BatteryReading(val percent: Int, val voltageMv: Int?, val charging: Boolean)
class PhoneTelemetry(private val config: ApiConfig, private val outbox: SyncOutbox) {
    private var lastSample: Long? = null
    fun sample(p: PhoneSensorPacket, battery: BatteryReading?, elapsedMs: Long): SensorRequest? {
        if (battery == null || battery.percent !in 0..100 ||
            listOf(p.accelXMs2, p.accelYMs2, p.accelZMs2).any { !it.isFinite() } || p.wallClockTimestampMs < 0) return null
        if (lastSample?.let { elapsedMs - it < 1000 } == true) return null
        lastSample = elapsedMs
        val gyro = listOf(p.gyroXDps, p.gyroYDps, p.gyroZDps).takeIf { it.all { v -> v != null && v.isFinite() } }
        return SensorRequest(config.deviceId, outbox.nextSequence(), p.wallClockTimestampMs,
            p.accelXMs2, p.accelYMs2, p.accelZMs2, gyro?.get(0), gyro?.get(1), gyro?.get(2),
            p.pressurePa?.takeIf { it.isFinite() && it > 0 }, p.altitudeDeltaM?.takeIf { it.isFinite() },
            battery.percent, battery.voltageMv, battery.charging, p.sensorQuality.coerceIn(0, 100), p.phoneMotionState)
    }
}
/** Sticky Android battery broadcast; no fake location or assumed sensor health. */
class AndroidDeviceInfo(private val context: Context) {
    fun battery(): BatteryReading? {
        val b = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED)) ?: return null
        val level = b.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
        val scale = b.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
        val status = b.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
        if (level < 0 || scale <= 0 || status !in listOf(BatteryManager.BATTERY_STATUS_CHARGING,
                BatteryManager.BATTERY_STATUS_FULL, BatteryManager.BATTERY_STATUS_DISCHARGING,
                BatteryManager.BATTERY_STATUS_NOT_CHARGING)) return null
        return BatteryReading((level * 100 / scale).coerceIn(0, 100),
            b.getIntExtra(BatteryManager.EXTRA_VOLTAGE, -1).takeIf { it > 0 },
            status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL)
    }
    fun heartbeat(battery: BatteryReading, packet: PhoneSensorPacket?): HeartbeatRequest {
        val fresh = packet?.takeIf { SystemClock.elapsedRealtimeNanos() - it.timestampNs in 0..500_000_000 }
        return HeartbeatRequest(BuildConfig.VERSION_NAME, System.currentTimeMillis(), SystemClock.elapsedRealtime() / 1000,
            battery.percent, battery.voltageMv, battery.charging,
            if (fresh != null) "OK" else "UNAVAILABLE", if (fresh?.pressurePa != null) "OK" else "UNAVAILABLE")
    }
}
