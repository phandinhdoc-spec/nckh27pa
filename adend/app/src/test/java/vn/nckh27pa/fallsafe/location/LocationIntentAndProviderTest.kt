package vn.nckh27pa.fallsafe.location

import org.junit.Assert.*
import org.junit.Test

class LocationIntentAndProviderTest {
    @Test fun googleMapsInstalledUsesExplicitGoogleMapsFirst() {
        val attempted = mutableListOf<MapTarget>()
        val result = MapLaunchResolver { target -> attempted += target; target == MapTarget.GOOGLE_MAPS }
            .open(10.5, 106.5, "https://maps.example")

        assertTrue(result.opened)
        assertEquals(MapTarget.GOOGLE_MAPS, result.target)
        assertEquals(listOf(MapTarget.GOOGLE_MAPS), attempted)
    }

    @Test fun missingGoogleMapsFallsBackToGenericGeo() {
        val attempted = mutableListOf<MapTarget>()
        val result = MapLaunchResolver { target -> attempted += target; target == MapTarget.GENERIC_MAPS }
            .open(10.5, 106.5, "https://maps.example")

        assertTrue(result.opened)
        assertEquals(MapTarget.GENERIC_MAPS, result.target)
        assertEquals(listOf(MapTarget.GOOGLE_MAPS, MapTarget.GENERIC_MAPS), attempted)
    }

    @Test fun noMapsAppOrBrowserReturnsFactualFailureWithoutThrowing() {
        val result = MapLaunchResolver { false }.open(10.5, 106.5, "https://maps.example")

        assertFalse(result.opened)
        assertNull(result.target)
        assertTrue(result.reason!!.contains("Không có ứng dụng"))
    }

    @Test fun bestAvailableUsesFusedBeforeGpsAndSupportsCoarseOnly() = kotlinx.coroutines.test.runTest {
        for (permission in LocationPermission.entries.filter { it != LocationPermission.DENIED }) {
            val attempted = mutableListOf<LocationProviderKind>()
            val source = object : PlatformLocationSource {
                override fun permission() = permission
                override fun enabledProviders() = if (permission == LocationPermission.PRECISE)
                    LocationProviderKind.entries.toSet() else setOf(LocationProviderKind.FUSED, LocationProviderKind.NETWORK)
                override suspend fun lastKnown(): vn.nckh27pa.fallsafe.emergency.LocationFix? = null
                override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): vn.nckh27pa.fallsafe.emergency.LocationFix? {
                    attempted += kind
                    return vn.nckh27pa.fallsafe.emergency.LocationFix.validated(10.5, 106.5, 150f, 1000,
                        vn.nckh27pa.fallsafe.emergency.LocationSource.UNKNOWN)
                }
            }
            val result = BestAvailableLocationRepository(source) { 1000 }.getBestAvailableLocation()
            assertEquals(listOf(LocationProviderKind.FUSED), attempted)
            assertEquals(vn.nckh27pa.fallsafe.emergency.LocationSource.FUSED, result.fix!!.source)
        }
    }
}
