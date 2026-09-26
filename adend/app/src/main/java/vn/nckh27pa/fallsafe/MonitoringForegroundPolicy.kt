package vn.nckh27pa.fallsafe

object MonitoringForegroundPolicy {
    fun includeLocationType(fineGranted: Boolean, coarseGranted: Boolean): Boolean = fineGranted || coarseGranted
}
