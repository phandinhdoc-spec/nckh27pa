package vn.nckh27pa.fallsafe

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Handler
import android.os.Looper
import android.os.SystemClock

/** Activity starts/stops collection; all callbacks are confined to main looper. */
class PhoneSensorCollector(context: Context, private val onPacket: (PhoneSensorPacket?) -> Unit) : SensorEventListener {
    private val manager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private val pipeline = PhoneInputPipeline(onPacket)
    private val kinds = mapOf(
        Sensor.TYPE_ACCELEROMETER to SensorKind.ACCEL,
        Sensor.TYPE_LINEAR_ACCELERATION to SensorKind.LINEAR,
        Sensor.TYPE_GYROSCOPE to SensorKind.GYRO,
        Sensor.TYPE_ROTATION_VECTOR to SensorKind.ORIENTATION,
        Sensor.TYPE_PRESSURE to SensorKind.PRESSURE
    )
    var activeSensors: Set<SensorKind> = emptySet(); private set
    private var running = false
    fun start() {
        stop()
        running = true
        activeSensors = kinds.mapNotNull { (type, kind) ->
            val sensor = manager.getDefaultSensor(type) ?: return@mapNotNull null
            val registered = try {
                manager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_GAME, Handler(Looper.getMainLooper()))
            } catch (_: SecurityException) { false }
            if (registered) kind else null
        }.toSet()
    }
    fun stop() {
        running = false
        manager.unregisterListener(this)
        pipeline.clear()
        activeSensors = emptySet()
    }
    fun latest(): PhoneSensorPacket? = pipeline.latest(SystemClock.elapsedRealtimeNanos(), System.currentTimeMillis())
    override fun onSensorChanged(event: SensorEvent) {
        if (!running) return
        val kind = kinds[event.sensor.type] ?: return
        if (kind == SensorKind.ORIENTATION) {
            if (event.values.size < 3 || event.values.any { !it.isFinite() }) {
                update(kind, event.timestamp, floatArrayOf(Float.NaN)); return
            }
            val rotation = FloatArray(9)
            val orientation = FloatArray(3)
            SensorManager.getRotationMatrixFromVector(rotation, event.values)
            SensorManager.getOrientation(rotation, orientation)
            update(kind, event.timestamp, floatArrayOf(
                Math.toDegrees(orientation[1].toDouble()).toFloat(),
                Math.toDegrees(orientation[2].toDouble()).toFloat(),
                Math.toDegrees(orientation[0].toDouble()).toFloat()
            ))
        } else update(kind, event.timestamp, event.values)
    }
    private fun update(kind: SensorKind, ns: Long, values: FloatArray) {
        pipeline.update(kind, ns, values, SystemClock.elapsedRealtimeNanos(), System.currentTimeMillis())
    }
    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) = Unit
}
