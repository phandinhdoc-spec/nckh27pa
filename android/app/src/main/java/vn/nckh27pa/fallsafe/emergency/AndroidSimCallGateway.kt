package vn.nckh27pa.fallsafe.emergency

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.core.content.ContextCompat
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

class AndroidSimCallGateway(private val context: Context) : EmergencyCallGateway {
    override fun call(phone: String): CallDispatchState {
        val state = try {
            when {
                ContextCompat.checkSelfPermission(context, Manifest.permission.CALL_PHONE) != PackageManager.PERMISSION_GRANTED ->
                    CallDispatchState(CallStatus.PERMISSION_MISSING, EmergencyFailureMessages.callPermissionMissing)
                !context.packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_CALLING) ->
                    CallDispatchState(CallStatus.UNAVAILABLE, "Thiết bị không hỗ trợ cuộc gọi SIM.")
                phone.isBlank() -> CallDispatchState(CallStatus.UNAVAILABLE, "Chưa có số điện thoại để gọi.")
                else -> {
                    val encodedPhone = Uri.encode(phone)
                    val intent = Intent(Intent.ACTION_CALL, Uri.parse("tel:$encodedPhone")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    if (Looper.myLooper() == Looper.getMainLooper()) {
                        launch(intent)
                    } else {
                        try {
                            val completed = CountDownLatch(1)
                            val launched = AtomicReference<CallDispatchState>()
                            val handler = Handler(Looper.getMainLooper())
                            val runnable = Runnable { launched.set(launch(intent)); completed.countDown() }
                            if (!handler.post(runnable)) CallDispatchState(CallStatus.FAILED, "Không thể gửi yêu cầu gọi SIM tới luồng chính.")
                            else if (completed.await(2, TimeUnit.SECONDS)) launched.get()
                                ?: CallDispatchState(CallStatus.FAILED, "Không nhận được kết quả mở cuộc gọi SIM.")
                            else {
                                handler.removeCallbacks(runnable)
                                CallDispatchState(CallStatus.FAILED, "Hết thời gian chờ mở cuộc gọi SIM.")
                            }
                        } catch (_: RuntimeException) {
                            CallDispatchState(CallStatus.FAILED, "Không thể gửi yêu cầu gọi SIM tới luồng chính.")
                        } catch (_: InterruptedException) {
                            Thread.currentThread().interrupt()
                            CallDispatchState(CallStatus.FAILED, "Đã gián đoạn khi mở cuộc gọi SIM.")
                        }
                    }
                }
            }
        } catch (_: SecurityException) {
            CallDispatchState(CallStatus.PERMISSION_MISSING, "Hệ thống từ chối quyền gọi điện.")
        } catch (_: ActivityNotFoundException) {
            CallDispatchState(CallStatus.FAILED, "Không có ứng dụng gọi điện.")
        } catch (_: RuntimeException) {
            CallDispatchState(CallStatus.UNAVAILABLE, "Không thể mở cuộc gọi SIM lúc này.")
        }
        log(state)
        return state
    }
    private fun launch(intent: Intent): CallDispatchState = try {
        context.startActivity(intent)
        CallDispatchState(CallStatus.STARTED, "Đã mở cuộc gọi SIM.")
    } catch (_: SecurityException) {
        CallDispatchState(CallStatus.PERMISSION_MISSING, "Hệ thống từ chối quyền gọi điện.")
    } catch (_: ActivityNotFoundException) {
        CallDispatchState(CallStatus.FAILED, "Không có ứng dụng gọi điện.")
    } catch (_: RuntimeException) {
        CallDispatchState(CallStatus.UNAVAILABLE, "Không thể mở cuộc gọi SIM lúc này.")
    }
    private fun log(state: CallDispatchState) {
        if (state.status == CallStatus.STARTED) Log.i("FallSafe/CALL", "status=${state.status}")
        else Log.w("FallSafe/CALL", "status=${state.status}")
    }
}
