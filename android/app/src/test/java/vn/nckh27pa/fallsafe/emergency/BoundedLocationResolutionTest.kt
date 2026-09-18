package vn.nckh27pa.fallsafe.emergency

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Test

class BoundedLocationResolutionTest {
    @Test fun timeoutUsesValidLastKnownFallbackAndIgnoresLateCurrentFix() {
        val fallback = LocationFix.validated(10.1, 106.1, 20f, 1_000, LocationSource.PHONE)!!
        val late = LocationFix.validated(10.2, 106.2, 5f, 2_000, LocationSource.PHONE)!!
        val completed = mutableListOf<LocationResolution>()
        val attempt = BoundedLocationResolutionAttempt({ fallback }, completed::add)

        attempt.fail(LocationFailureCause.TIMEOUT)
        attempt.current(late)

        assertEquals(1, completed.size)
        assertSame(fallback, completed.single().fix)
        assertEquals(LocationFailureCause.TIMEOUT, completed.single().cause)
    }

    @Test fun disabledProviderAndNullOrInvalidCurrentFixCompleteWithoutCoordinates() {
        for (cause in listOf(
            LocationFailureCause.PROVIDER_DISABLED,
            LocationFailureCause.NO_FIX,
            LocationFailureCause.INVALID_FIX
        )) {
            var resolution: LocationResolution? = null
            val attempt = BoundedLocationResolutionAttempt({ null }) { resolution = it }
            attempt.fail(cause)
            assertNull(resolution!!.fix)
            assertEquals(cause, resolution!!.cause)
        }
    }

    @Test fun freshCurrentFixWinsOverLastKnownFallback() {
        val fallback = LocationFix.validated(10.1, 106.1, 20f, 1_000, LocationSource.PHONE)!!
        val current = LocationFix.validated(10.2, 106.2, 5f, 2_000, LocationSource.PHONE)!!
        var resolution: LocationResolution? = null
        val attempt = BoundedLocationResolutionAttempt({ fallback }) { resolution = it }

        attempt.current(current)

        assertSame(current, resolution!!.fix)
        assertNull(resolution!!.cause)
    }
}
