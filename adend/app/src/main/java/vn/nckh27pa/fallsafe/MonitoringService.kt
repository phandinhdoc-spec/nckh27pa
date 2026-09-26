package vn.nckh27pa.fallsafe

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.PowerManager
import core.State
import android.Manifest
import androidx.core.content.ContextCompat

/** Explicitly user-started demo monitoring; no boot receiver or hidden restart. */
class MonitoringService : Service() {
    private val controller get() = (application as DemoApplication).controller
    private lateinit var collector: PhoneSensorCollector
    private lateinit var wake: PowerManager.WakeLock
    private var protectedEvent: Long? = null
    private val handler = Handler(Looper.getMainLooper())
    private val poll = object : Runnable {
        override fun run() {
            controller.displayPhone(collector.latest())
            controller.session.tick()
            controller.refresh()
            handler.postDelayed(this, 250)
        }
    }
    override fun onCreate() {
        super.onCreate()
        val manager = getSystemService(NotificationManager::class.java)
        manager.createNotificationChannel(NotificationChannel("monitor", "Giám sát thử nghiệm", NotificationManager.IMPORTANCE_LOW))
        if (!manager.areNotificationsEnabled()) {
            controller.backgroundMessage = "Chưa bật giám sát nền vì thông báo hệ thống bị tắt; mở quyền thông báo để tiếp tục."
            stopSelf(); return
        }
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val notificationBuilder = Notification.Builder(this, "monitor")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentTitle("FallSafe — GIÁM SÁT THỬ NGHIỆM")
            .setContentText("Đọc cảm biến điện thoại; không liên hệ người thật. Mở app để dừng.")
            .setContentIntent(open).setOngoing(true)
        if (Build.VERSION.SDK_INT >= 31) notificationBuilder.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE)
        val notification = notificationBuilder.build()
        try {
            if (Build.VERSION.SDK_INT >= 34) {
                val fine=ContextCompat.checkSelfPermission(this,Manifest.permission.ACCESS_FINE_LOCATION)==PackageManager.PERMISSION_GRANTED
                val coarse=ContextCompat.checkSelfPermission(this,Manifest.permission.ACCESS_COARSE_LOCATION)==PackageManager.PERMISSION_GRANTED
                val type=ServiceInfo.FOREGROUND_SERVICE_TYPE_HEALTH or
                    (if(MonitoringForegroundPolicy.includeLocationType(fine,coarse))ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION else 0)
                startForeground(27, notification, type)
            }
            else startForeground(27, notification)
        } catch (_: RuntimeException) {
            controller.backgroundMessage = "Không thể bật giám sát nền; kiểm tra quyền/hệ thống."
            stopSelf(); return
        }
        wake = getSystemService(PowerManager::class.java).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "nckh27pa:verification")
        wake.setReferenceCounted(false)
        collector = PhoneSensorCollector(this, controller::acceptPhone)
        controller.backgroundMonitoring = true
        controller.onMonitoringChanged?.invoke()
        controller.backgroundMessage = if (manager.areNotificationsEnabled()) "Giám sát nền thử đang bật; có thông báo hệ thống."
            else "Giám sát nền đang bật, nhưng quyền/kênh thông báo bị tắt; xem mục ứng dụng đang chạy của Android."
        controller.onStateChanged = ::protectVerification
        collector.start()
        controller.backgroundSensors = collector.activeSensors.joinToString()
        protectVerification()
        handler.post(poll)
    }
    private fun protectVerification() {
        val state = controller.snapshot
        if (state.state == State.VERIFYING) {
            if (protectedEvent != state.eventId) {
                protectedEvent = state.eventId
                wake.acquire(((state.remainingMs ?: 0).coerceIn(0, 10000) + 1000))
            }
        } else {
            protectedEvent = null
            if (wake.isHeld) wake.release()
        }
    }
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = START_NOT_STICKY
    override fun onBind(intent: Intent?): IBinder? = null
    override fun onDestroy() {
        handler.removeCallbacks(poll)
        if (::collector.isInitialized) {
            controller.onStateChanged = null
            collector.stop()
            if (::wake.isInitialized && wake.isHeld) wake.release()
            controller.backgroundMonitoring = false
            controller.backgroundSensors = ""
            controller.backgroundMessage = "Giám sát nền đã dừng; mở app để thu cảm biến."
            controller.displayPhone(null)
            controller.session.resetDetection()
            // Notify after cleanup; never rely on a fixed delay for handoff.
            controller.onMonitoringChanged?.invoke()
        }
        stopForeground(STOP_FOREGROUND_REMOVE)
        super.onDestroy()
    }
}
