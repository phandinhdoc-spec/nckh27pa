package vn.nckh27pa.fallsafe

class SensorOwnership(private val startLocal: () -> Unit, private val stopLocal: () -> Unit) {
    private var localActive = false
    fun update(foreground: Boolean, serviceActive: Boolean) {
        val wanted = foreground && !serviceActive
        if (wanted == localActive) return
        localActive = wanted
        if (wanted) startLocal() else stopLocal()
    }
}
