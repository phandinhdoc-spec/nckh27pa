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

    @Test fun coarseOnlyNeverChoosesGpsAndUsesCoarseCapableProvider() {
        assertEquals(
            LocationProvider.NETWORK,
            LocationProviderSelector.choose(hasFine = false, gpsEnabled = true, networkEnabled = true, fusedEnabled = true)
        )
        assertEquals(
            LocationProvider.FUSED,
            LocationProviderSelector.choose(hasFine = false, gpsEnabled = true, networkEnabled = false, fusedEnabled = true)
        )
        assertNull(LocationProviderSelector.choose(hasFine = false, gpsEnabled = true, networkEnabled = false, fusedEnabled = false))
        assertEquals(
            LocationProvider.GPS,
            LocationProviderSelector.choose(hasFine = true, gpsEnabled = true, networkEnabled = true, fusedEnabled = true)
        )
    }
}
