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
import vn.nckh27pa.fallsafe.permissions.resolveTelephonySupport

class AndroidSimCallGateway(private val context: Context) : EmergencyCallGateway {
    override fun call(phone: String): CallDispatchState {
        val state = try {
            when {
                ContextCompat.checkSelfPermission(context, Manifest.permission.CALL_PHONE) != PackageManager.PERMISSION_GRANTED -> {
                    // TEMPORARY DIAGNOSTIC
                    vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "checkSelfPermission(CALL_PHONE)!=PERMISSION_GRANTED")
                    vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = false, intentCreated = false, startActivityReached = false, startActivityReturned = false, exception = "none")
                    CallDispatchState(CallStatus.PERMISSION_MISSING, EmergencyFailureMessages.callPermissionMissing)
                }
                !resolveTelephonySupport(
                    hasBaseTelephony = context.packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY),
                    hasGranularFeature = context.packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_CALLING)
                ) -> {
                    // TEMPORARY DIAGNOSTIC
                    vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "FEATURE_TELEPHONY_CALLING=false")
                    vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = phone.isNotBlank(), permission = ContextCompat.checkSelfPermission(context, Manifest.permission.CALL_PHONE) == PackageManager.PERMISSION_GRANTED, intentCreated = false, startActivityReached = false, startActivityReturned = false, exception = "none")
                    CallDispatchState(CallStatus.UNAVAILABLE, "Thiết bị không hỗ trợ cuộc gọi SIM.")
                }
                phone.isBlank() -> {
                    // TEMPORARY DIAGNOSTIC
                    vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "phone_blank")
                    vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = false, permission = true, intentCreated = false, startActivityReached = false, startActivityReturned = false, exception = "none")
                    CallDispatchState(CallStatus.UNAVAILABLE, "Chưa có số điện thoại để gọi.")
                }
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
        // TEMPORARY DIAGNOSTIC
        vn.nckh27pa.fallsafe.AndroidTrace.logPermission(context)
        vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = true, permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = false, exception = "none")
        context.startActivity(intent)
        vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = true, permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = true, exception = "none")
        CallDispatchState(CallStatus.STARTED, "Đã mở cuộc gọi SIM.")
    } catch (e: SecurityException) {
        // TEMPORARY DIAGNOSTIC
        vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "SecurityException: ${e.message}")
        vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = true, permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = false, exception = e.javaClass.simpleName)
        CallDispatchState(CallStatus.PERMISSION_MISSING, "Hệ thống từ chối quyền gọi điện.")
    } catch (e: ActivityNotFoundException) {
        // TEMPORARY DIAGNOSTIC
        vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "ActivityNotFoundException: ${e.message}")
        vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = true, permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = false, exception = e.javaClass.simpleName)
        CallDispatchState(CallStatus.FAILED, "Không có ứng dụng gọi điện.")
    } catch (e: RuntimeException) {
        // TEMPORARY DIAGNOSTIC
        vn.nckh27pa.fallsafe.AndroidTrace.logBlocked("CALL", "RuntimeException: ${e.message}")
        vn.nckh27pa.fallsafe.AndroidTrace.logCall(entered = true, contactExists = true, phonePresent = true, permission = true, intentCreated = true, startActivityReached = true, startActivityReturned = false, exception = e.javaClass.simpleName)
        CallDispatchState(CallStatus.UNAVAILABLE, "Không thể mở cuộc gọi SIM lúc này.")
    }
    private fun log(state: CallDispatchState) {
        if (state.status == CallStatus.STARTED) Log.i("FallSafe/CALL", "status=${state.status}")
        else Log.w("FallSafe/CALL", "status=${state.status}")
    }
}
