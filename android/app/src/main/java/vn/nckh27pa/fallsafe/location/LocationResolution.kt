package vn.nckh27pa.fallsafe.location

enum class LocationProvider { GPS, NETWORK, FUSED }

object LocationProviderSelector {
    fun choose(
        hasFine: Boolean,
        gpsEnabled: Boolean,
        networkEnabled: Boolean,
        fusedEnabled: Boolean
    ): LocationProvider? = when {
        hasFine && gpsEnabled -> LocationProvider.GPS
        networkEnabled -> LocationProvider.NETWORK
        fusedEnabled -> LocationProvider.FUSED
        else -> null
    }
}

enum class MapTarget { GOOGLE_MAPS, GENERIC_MAPS, BROWSER }
data class MapOpenResult(val opened: Boolean, val target: MapTarget? = null, val reason: String? = null)
fun interface MapTargetLauncher { fun launch(target: MapTarget): Boolean }

class MapLaunchResolver(
    private val failureReason: () -> String? = { null },
    private val launcher: MapTargetLauncher
) {
    fun open(latitude: Double, longitude: Double, mapsUrl: String): MapOpenResult {
        if (!latitude.isFinite() || !longitude.isFinite() || mapsUrl.isBlank()) {
            return MapOpenResult(false, reason = "Chưa có vị trí hợp lệ để mở bản đồ.")
        }
        for (target in listOf(MapTarget.GOOGLE_MAPS, MapTarget.GENERIC_MAPS, MapTarget.BROWSER)) {
            val opened = try { launcher.launch(target) } catch (_: RuntimeException) { false }
            if (opened) return MapOpenResult(true, target)
        }
        return MapOpenResult(false, reason = failureReason() ?: "Không có ứng dụng phù hợp để mở bản đồ.")
    }
}
