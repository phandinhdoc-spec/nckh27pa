package vn.nckh27pa.fallsafe

import androidx.lifecycle.ViewModel

/** Activity-owned UI entry point. Local validation completes synchronously; the application
 * coordinator persists mutations and owns network work across Activity recreation. */
class ContactsViewModel(private val controller: DemoController) : ViewModel() {
    fun add(name: String, relationship: String, phone: String, receiveSos: Boolean, primary: Boolean) =
        controller.addContact(name, relationship, phone, receiveSos, primary)
    fun update(contact: EmergencyContact) = controller.updateContact(contact)
    fun delete(id: String) = controller.deleteContact(id)
    fun primary(id: String) = controller.setPrimaryContact(id)
    fun toggle(id: String) = controller.toggleReceiveSos(id)
}
