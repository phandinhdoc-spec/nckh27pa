package vn.nckh27pa.fallsafe.location

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import vn.nckh27pa.fallsafe.emergency.LocationFix
import vn.nckh27pa.fallsafe.emergency.LocationSource

class LocationRecoveryEngineTest {
    private fun fix(fixTimeMs: Long = 0L) =
        LocationFix.validated(10.5, 106.5, 10f, fixTimeMs, LocationSource.FUSED)!!

    private class CountingSource(
        var permission: LocationPermission = LocationPermission.PRECISE,
        var providers: Set<LocationProviderKind> = LocationProviderKind.entries.toSet(),
        var cached: LocationFix? = null,
        var currentFix: LocationFix? = null
    ) : PlatformLocationSource {
        var lastKnownCalls = 0
        var currentCalls = 0
        override fun permission() = permission
        override fun enabledProviders() = providers
        override suspend fun lastKnown(): LocationFix? { lastKnownCalls++; return cached }
        override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix? { currentCalls++; return currentFix }
    }

    @Test fun startupWithLocationOnAcquiresAndPublishesFix() = runTest {
        val source = CountingSource(currentFix = fix())
        val engine = LocationRecoveryEngine(source, LocationRecoveryPolicy(), { 0L })
        val evaluation = engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false)
        assertEquals(LocationRecoveryDecision.Acquire, evaluation.decision)
        assertTrue(evaluation.availabilityChanged)
        val lookup = engine.lookup()
        assertNotNull(lookup.fix)
    }

    @Test fun startupWithLocationOffIsProviderDisabledWithZeroProviderCalls() = runTest {
        val source = CountingSource(providers = emptySet())
        val engine = LocationRecoveryEngine(source, LocationRecoveryPolicy(), { 0L })
        val evaluation = engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false)
        assertEquals(LocationRecoveryDecision.ProviderDisabled, evaluation.decision)
        assertEquals(0, source.lastKnownCalls)
        assertEquals(0, source.currentCalls)
    }

    @Test fun offToOnRecoversWithSingleAcquireAndLookup() = runTest {
        val source = CountingSource(providers = emptySet())
        val engine = LocationRecoveryEngine(source, LocationRecoveryPolicy(), { 0L })
        assertEquals(
            LocationRecoveryDecision.ProviderDisabled,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false).decision
        )
        source.providers = LocationProviderKind.entries.toSet()
        source.currentFix = fix()
        val evaluation = engine.evaluate(LocationSignal.SYSTEM_LOCATION_CHANGED, requestInFlight = false, hasFix = false, fixIsFresh = false)
        assertEquals(LocationRecoveryDecision.Acquire, evaluation.decision)
        assertTrue(evaluation.availabilityChanged)
        val lookup = engine.lookup()
        assertNotNull(lookup.fix)
        assertEquals(1, source.currentCalls)
    }

    @Test fun resumeAfterSettingsChangeRecovers() = runTest {
        val source = CountingSource(providers = emptySet())
        val engine = LocationRecoveryEngine(source, LocationRecoveryPolicy(), { 0L })
        assertEquals(
            LocationRecoveryDecision.ProviderDisabled,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false).decision
        )
        source.providers = LocationProviderKind.entries.toSet()
        source.currentFix = fix()
        val evaluation = engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false)
        assertEquals(LocationRecoveryDecision.Acquire, evaluation.decision)
        assertNotNull(engine.lookup().fix)
        assertEquals(1, source.currentCalls)
    }

    @Test fun repeatedResumeHasNoOverlapAndRespectsCooldown() = runTest {
        var now = 0L
        val source = CountingSource(currentFix = null)
        val engine = LocationRecoveryEngine(source, LocationRecoveryPolicy({ now }), { now })

        // startup acquires; a resume while the request is still in flight must not overlap
        assertEquals(
            LocationRecoveryDecision.Acquire,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false).decision
        )
        source.currentFix = fix(now)
        assertEquals(
            LocationRecoveryDecision.None,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = true, hasFix = false, fixIsFresh = false).decision
        )
        assertNotNull(engine.lookup().fix)
        val callsAfterFix = source.currentCalls

        // repeated resumes with a fresh fix cause zero further lookups
        assertEquals(
            LocationRecoveryDecision.None,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = true, fixIsFresh = true).decision
        )
        assertEquals(callsAfterFix, source.currentCalls)

        // failed attempt: no fix; resumes inside the cooldown cause none, after the cooldown exactly one
        now = 30_000L
        source.currentFix = null
        assertEquals(
            LocationRecoveryDecision.Acquire,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false).decision
        )
        assertNull(engine.lookup().fix)
        assertEquals(
            LocationRecoveryDecision.None,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false).decision
        )
        now = 60_000L
        assertEquals(
            LocationRecoveryDecision.Acquire,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false).decision
        )
    }

    @Test fun permissionDeniedThenGrantedRecovers() = runTest {
        val source = CountingSource(permission = LocationPermission.DENIED)
        val engine = LocationRecoveryEngine(source, LocationRecoveryPolicy(), { 0L })
        assertEquals(
            LocationRecoveryDecision.PermissionDenied,
            engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false).decision
        )
        assertEquals(0, source.lastKnownCalls)
        assertEquals(0, source.currentCalls)
        source.permission = LocationPermission.PRECISE
        source.currentFix = fix()
        val evaluation = engine.evaluate(LocationSignal.FOREGROUND_RESUMED, requestInFlight = false, hasFix = false, fixIsFresh = false)
        assertEquals(LocationRecoveryDecision.Acquire, evaluation.decision)
        assertNotNull(engine.lookup().fix)
        assertEquals(1, source.currentCalls)
    }
}
