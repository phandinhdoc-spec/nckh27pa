package vn.nckh27pa.fallsafe.emergency

import android.Manifest
import android.app.Activity
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.telephony.SmsManager
import android.telephony.SubscriptionManager
import androidx.core.content.ContextCompat
import java.io.Closeable
import java.util.concurrent.ConcurrentHashMap

class AndroidSmsManagerGateway(private val context:Context) : EmergencySmsGateway, Closeable {
    private val parts=ConcurrentHashMap<String,SmsPartAggregation>()
    private val states=ConcurrentHashMap<String,SmsDispatchState>()
    @Volatile var onStateChanged: ((SmsDispatchState) -> Unit)? = null
    private val sentAction="${context.packageName}.SMS_SENT"
    private val deliveredAction="${context.packageName}.SMS_DELIVERED"
    private val receiver=object:BroadcastReceiver(){override fun onReceive(c:Context?,intent:Intent?){
        val event=intent?.getStringExtra("event")?:return
        val contact=intent.getStringExtra("contact")?:return
        val index=intent.getIntExtra("part",-1)
        val key="$event:$contact";val aggregate=parts[key]?:return
        if(index<0)return
        val result=if(intent.action==sentAction)aggregate.recordSent(index,resultCode==Activity.RESULT_OK)
            else aggregate.recordDelivery(index,resultCode==Activity.RESULT_OK)
        states[key]=SmsDispatchState(event,contact,result.status,result.detail).also { onStateChanged?.invoke(it) }
    }}
    init {
        val filter=IntentFilter().apply{addAction(sentAction);addAction(deliveredAction)}
        if(Build.VERSION.SDK_INT>=33)context.registerReceiver(receiver,filter,Context.RECEIVER_NOT_EXPORTED) else @Suppress("DEPRECATION") context.registerReceiver(receiver,filter)
    }
    fun state(eventId:String,contactId:String)=states["$eventId:$contactId"]
    override fun send(request:SmsRequest):SmsDispatchState {
        fun fail(reason:String)=SmsDispatchState(request.eventId,request.contactId,SmsDeliveryStatus.FAILED,reason).also{
            states["${request.eventId}:${request.contactId}"]=it;onStateChanged?.invoke(it)
        }
        if(!context.packageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_MESSAGING))return fail("Thiết bị không hỗ trợ SMS di động")
        if(ContextCompat.checkSelfPermission(context,Manifest.permission.SEND_SMS)!=PackageManager.PERMISSION_GRANTED)return fail(EmergencyFailureMessages.messagingPermissionMissing)
        val hasPhoneStateAccess=ContextCompat.checkSelfPermission(context,Manifest.permission.READ_PHONE_STATE)==PackageManager.PERMISSION_GRANTED
        val subscription=try {
            val manager=context.getSystemService(SubscriptionManager::class.java)
            if(hasPhoneStateAccess&&manager.activeSubscriptionInfoCount>1&&request.subscriptionId==null)return fail("Có nhiều SIM; cần chọn SIM gửi rõ ràng")
            request.subscriptionId?:SmsManager.getDefaultSmsSubscriptionId().takeIf{it!=SubscriptionManager.INVALID_SUBSCRIPTION_ID}
        }catch(_:SecurityException){request.subscriptionId}
        return try {
            val manager=if(subscription==null) @Suppress("DEPRECATION") SmsManager.getDefault()
                else if(Build.VERSION.SDK_INT>=31)context.getSystemService(SmsManager::class.java).createForSubscriptionId(subscription)
                else @Suppress("DEPRECATION") SmsManager.getSmsManagerForSubscriptionId(subscription)
            val bodies=manager.divideMessage(request.message)
            if(bodies.isEmpty())return fail("Nội dung SMS trống")
            val key="${request.eventId}:${request.contactId}";parts[key]=SmsPartAggregation(bodies.size)
            states[key]=SmsDispatchState(request.eventId,request.contactId,SmsDeliveryStatus.QUEUED)
            val sent=ArrayList<PendingIntent>(bodies.size);val delivered=ArrayList<PendingIntent>(bodies.size)
            bodies.indices.forEach{index->sent+=callback(sentAction,request,index);delivered+=callback(deliveredAction,request,index)}
            states[key]=SmsDispatchState(request.eventId,request.contactId,SmsDeliveryStatus.SENDING)
            manager.sendMultipartTextMessage(request.phone,null,bodies,sent,delivered);states[key]!!
        } catch(_:SecurityException) { fail("Hệ thống từ chối gửi SMS") }
        catch(_:RuntimeException){fail("Không thể xếp hàng SMS")}
    }
    private fun callback(action:String,request:SmsRequest,index:Int):PendingIntent {
        val intent=Intent(action).setPackage(context.packageName)
            .setData(Uri.parse("fallsafe://sms/${request.eventId}/${request.contactId}/$index/$action"))
            .putExtra("event",request.eventId).putExtra("contact",request.contactId).putExtra("part",index)
        return PendingIntent.getBroadcast(context,("$action:${request.eventId}:${request.contactId}:$index").hashCode(),intent,PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }
    override fun close(){try{context.unregisterReceiver(receiver)}catch(_:IllegalArgumentException){}}
}
