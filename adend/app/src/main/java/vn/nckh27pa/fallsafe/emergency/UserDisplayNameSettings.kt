package vn.nckh27pa.fallsafe.emergency

import android.content.Context

const val DEFAULT_USER_DISPLAY_NAME = "Người dùng FallSafe"
class UserDisplayNameSettings(context:Context) {
    private val prefs=context.getSharedPreferences("fallsafe_settings",Context.MODE_PRIVATE)
    var value:String
        get()=prefs.getString("user_display_name",null)?.trim().takeUnless{it.isNullOrEmpty()}?:DEFAULT_USER_DISPLAY_NAME
        set(name){prefs.edit().putString("user_display_name",name.trim().take(100).ifBlank{DEFAULT_USER_DISPLAY_NAME}).commit()}
}
