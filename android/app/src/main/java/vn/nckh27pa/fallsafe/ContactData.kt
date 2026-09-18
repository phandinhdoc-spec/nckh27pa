package vn.nckh27pa.fallsafe

import android.content.Context
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import java.util.UUID

/**
 * Data model for emergency contacts who receive SOS alerts and notifications.
 */
data class EmergencyContact(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val relationship: String,
    val phone: String,
    val receiveSos: Boolean = true,
    val isPrimary: Boolean = false,
    val callPriority: Int = Int.MAX_VALUE
)

object ContactValidator {
    /**
     * Normalizes a phone number by stripping whitespace, dashes, dots, and parentheses.
     */
    fun normalize(phone: String): String {
        return phone.replace(Regex("[\\s\\-\\.\\(\\)]"), "").trim()
    }

    /**
     * Validates a Vietnamese phone number.
     * Accepts numbers starting with '0' (10 digits) or '+84' (12 chars: +84 followed by 9 digits).
     * Returns null if valid, or a Vietnamese error message if invalid.
     */
    fun validate(phone: String): String? {
        val clean = normalize(phone)
        if (clean.isBlank()) {
            return "Số điện thoại không được để trống"
        }
        val vnRegex = Regex("^(\\+84[35789][0-9]{8}|0[35789][0-9]{8})$")
        return if (vnRegex.matches(clean)) {
            null
        } else {
            "Số điện thoại không đúng định dạng VN (10 số đầu 0 hoặc +84)"
        }
    }

    /**
     * Masks phone number in the format: 090…123 or +84…123.
     */
    fun mask(phone: String): String {
        val clean = normalize(phone)
        if (clean.startsWith("+84") && clean.length >= 8) {
            return clean.substring(0, 5) + "…" + clean.takeLast(3)
        }
        if (clean.length >= 6) {
            return clean.take(3) + "…" + clean.takeLast(3)
        }
        return clean
    }
}

interface ContactRepository {
    fun getContacts(): List<EmergencyContact>
    fun saveContacts(contacts: List<EmergencyContact>)
}

/**
 * Lightweight persistence using SharedPreferences & Gson.
 * Survives process death and app restart without adding heavy database libraries.
 */
class SharedPrefsContactRepository(context: Context, userId: String = BuildConfig.FALLSAFE_USER_ID) : ContactRepository {
    // New user-scoped store deliberately does not import legacy demo defaults.
    private val prefs = context.getSharedPreferences("fallsafe_contacts_v2_$userId", Context.MODE_PRIVATE)
    private val gson = Gson()
    private val key = "contacts_list_json"
    override fun getContacts(): List<EmergencyContact> = try {
        val type = object : TypeToken<List<EmergencyContact>>() {}.type
        prefs.getString(key, null)?.let { gson.fromJson<List<EmergencyContact>>(it, type) } ?: emptyList()
    } catch (_: Exception) { emptyList() }
    override fun saveContacts(contacts: List<EmergencyContact>) {
        prefs.edit().putString(key, gson.toJson(contacts)).apply()
    }
}

/**
 * In-memory repository for unit tests and Compose previews.
 */
class InMemoryContactRepository(initial: List<EmergencyContact> = emptyList()) : ContactRepository {
    private var list = initial.toList()
    override fun getContacts(): List<EmergencyContact> = list
    override fun saveContacts(contacts: List<EmergencyContact>) { list = contacts.toList() }
}
