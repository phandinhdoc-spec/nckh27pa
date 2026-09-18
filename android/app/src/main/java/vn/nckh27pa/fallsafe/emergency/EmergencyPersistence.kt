package vn.nckh27pa.fallsafe.emergency

import android.content.Context
import com.google.gson.Gson
import java.util.UUID

interface EventIdentityStore { fun id(localEventId:Long):String; fun clear(localEventId:Long) }
class SessionEventIdentityStore(private val namespace:String=UUID.randomUUID().toString()):EventIdentityStore {
    override fun id(localEventId:Long)=UUID.nameUUIDFromBytes("$namespace:$localEventId".toByteArray()).toString()
    override fun clear(localEventId:Long) { }
}
class SharedPrefsEventIdentityStore(context:Context):EventIdentityStore {
    private val prefs=context.getSharedPreferences("fallsafe_emergency_identity",Context.MODE_PRIVATE)
    @Synchronized override fun id(localEventId:Long):String {
        val existing=prefs.getString("uuid",null)
        if(prefs.getLong("local",-1)==localEventId&&existing!=null)return existing
        val created=UUID.randomUUID().toString();prefs.edit().putLong("local",localEventId).putString("uuid",created).commit();return created
    }
    @Synchronized override fun clear(localEventId:Long){if(prefs.getLong("local",-1)==localEventId)prefs.edit().clear().commit()}
}
class SharedPrefsEmergencyStore(context:Context):EmergencyStore {
    private val prefs=context.getSharedPreferences("fallsafe_emergency_records",Context.MODE_PRIVATE)
    private val gson=Gson()
    override fun get(eventId:String):EmergencyRecord?=try{prefs.getString(eventId,null)?.let{gson.fromJson(it,EmergencyRecord::class.java)}}catch(_:RuntimeException){null}
    override fun save(record:EmergencyRecord){prefs.edit().putString(record.eventId,gson.toJson(record)).commit()}
}
