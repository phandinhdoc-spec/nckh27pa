package vn.nckh27pa.fallsafe.location

import kotlinx.coroutines.delay
import kotlinx.coroutines.test.runTest
import org.junit.Assert.*
import org.junit.Test
import vn.nckh27pa.fallsafe.emergency.*

class BestAvailableLocationRepositoryTest {
    @Test fun cachedFixIsPublishedBeforeWaitingForCurrentFix() = runTest {
        val published = mutableListOf<LocationLookup>()
        val source = Fake().apply {
            cached = fix(199_000)
            fetch = { _, _ ->
                assertEquals(199_000L, published.single().fix!!.fixTimeMs)
                delay(100)
                fix(200_000)
            }
        }
        val lookup = BestAvailableLocationRepository(source, onCached = { published += it }) { 200_000 }
            .getBestAvailableLocation()
        assertNull(published.single().cause)
        assertTrue(published.single().fromCache)
        assertFalse(lookup.fromCache)
        assertEquals(200_000L, lookup.fix!!.fixTimeMs)
    }
    @Test fun playServicesIsNotAnEnabledLocationProviderWhenSystemLocationIsOff() {
        assertTrue(LocationProviderAvailability.enabled(
            playServicesAvailable = true,
            systemLocationEnabled = false,
            platformFusedEnabled = false,
            precisePermission = true,
            gpsEnabled = false,
            networkEnabled = false
        ).isEmpty())
    }
    private fun fix(time: Long = 200_000, accuracy: Float = 10f) =
        LocationFix.validated(10.5, 106.5, accuracy, time, LocationSource.CACHED)!!
    private class Fake : PlatformLocationSource {
        var access = LocationPermission.PRECISE
        var enabled = LocationProviderKind.entries.toSet()
        var cached: LocationFix? = null
        var cacheReads = 0
        val attempts = mutableListOf<Pair<LocationProviderKind, Long>>()
        var fetch: suspend (LocationProviderKind, Long) -> LocationFix? = { _, _ -> null }
        override fun permission() = access
        override fun enabledProviders() = enabled
        override suspend fun lastKnown(): LocationFix? { cacheReads++; return cached }
        override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix? {
            attempts += kind to timeoutMs
            return fetch(kind, timeoutMs)
        }
    }
    @Test fun freshCacheReturnsImmediatelyWithoutCurrentRequest() = runTest {
        val source = Fake().apply { cached = fix() }
        val lookup = BestAvailableLocationRepository(source) { 200_000 }.getBestAvailableLocation()
        assertTrue(lookup.fromCache); assertNull(lookup.cause)
        assertEquals(10.5, lookup.fix!!.latitude, 0.0)
        assertTrue(source.attempts.isEmpty()); assertEquals(1, source.cacheReads)
    }
    @Test fun staleCacheWaitsForCurrentAndUsesProviderSource() = runTest {
        val source = Fake().apply { cached = fix(1); fetch = { _, _ -> fix() } }
        val lookup = BestAvailableLocationRepository(source) { 200_000 }.getBestAvailableLocation()
        assertFalse(lookup.fromCache); assertNull(lookup.cause)
        assertEquals(LocationSource.FUSED, lookup.fix!!.source)
        assertEquals(listOf(LocationProviderKind.FUSED), source.attempts.map { it.first })
    }
    @Test fun gpsDisabledDoesNotPreventFusedFix() = runTest {
        val source = Fake().apply { enabled = setOf(LocationProviderKind.FUSED, LocationProviderKind.NETWORK); fetch = { _, _ -> fix() } }
        val lookup = BestAvailableLocationRepository(source) { 200_000 }.getBestAvailableLocation()
        assertNotNull(lookup.fix); assertEquals(LocationSource.FUSED, lookup.fix!!.source)
    }
    @Test fun gpsNullFallsThroughToNetworkAndAccepts150Meters() = runTest {
        val source = Fake().apply { fetch = { kind, _ -> if (kind == LocationProviderKind.NETWORK) fix(accuracy = 150f) else null } }
        val lookup = BestAvailableLocationRepository(source) { 200_000 }.getBestAvailableLocation()
        assertNull(lookup.cause); assertEquals(150f, lookup.fix!!.accuracyM)
        assertEquals(LocationSource.NETWORK, lookup.fix!!.source)
        assertEquals(LocationProviderKind.entries, source.attempts.map { it.first })
    }
    @Test fun noFixAnywhereHasNoFixCause() = runTest {
        val lookup = BestAvailableLocationRepository(Fake()) { 200_000 }.getBestAvailableLocation()
        assertNull(lookup.fix); assertEquals(LocationFailureCause.NO_FIX, lookup.cause)
    }
    @Test fun deniedPermissionSkipsAllLocationAccess() = runTest {
        val source = Fake().apply { access = LocationPermission.DENIED }
        val lookup = BestAvailableLocationRepository(source) { 200_000 }.getBestAvailableLocation()
        assertEquals(LocationFailureCause.PERMISSION_DENIED, lookup.cause)
        assertNull(lookup.fix); assertEquals(0, source.cacheReads); assertTrue(source.attempts.isEmpty())
    }
    @Test fun emptyProvidersReturnsDisabled() = runTest {
        val source = Fake().apply { enabled = emptySet() }
        val lookup = BestAvailableLocationRepository(source) { 200_000 }.getBestAvailableLocation()
        assertEquals(LocationFailureCause.PROVIDER_DISABLED, lookup.cause)
        assertEquals(0, source.cacheReads); assertTrue(source.attempts.isEmpty())
    }
    @Test fun staleCacheSurvivesTimeoutAndWholeLookupIsBounded() = runTest {
        val source = Fake().apply { cached = fix(1); fetch = { _, _ -> delay(20_000); null } }
        val lookup = BestAvailableLocationRepository(source) { 200_000 + testScheduler.currentTime }.getBestAvailableLocation(8000)
        assertNotNull(lookup.fix); assertTrue(lookup.fromCache)
        assertEquals(LocationFailureCause.TIMEOUT, lookup.cause)
        assertEquals(8000, testScheduler.currentTime); assertEquals(8000, lookup.elapsedMs)
    }
    @Test fun eachAttemptReceivesOnlyRemainingBudget() = runTest {
        val source = Fake().apply { fetch = { _, _ -> delay(100); null } }
        BestAvailableLocationRepository(source) { testScheduler.currentTime }.getBestAvailableLocation(1000)
        assertEquals(listOf(500L, 450L, 800L), source.attempts.map { it.second })
    }
    @Test fun cacheLookupIsAlsoBoundedAndExceptionsDoNotEscape() = runTest {
        val hanging = object : PlatformLocationSource {
            override fun permission() = LocationPermission.PRECISE
            override fun enabledProviders() = setOf(LocationProviderKind.FUSED)
            override suspend fun lastKnown(): LocationFix? { delay(20_000); return null }
            override suspend fun current(kind: LocationProviderKind, timeoutMs: Long): LocationFix? = error("unexpected")
        }
        val lookup = BestAvailableLocationRepository(hanging) { testScheduler.currentTime }.getBestAvailableLocation(500)
        assertNull(lookup.fix); assertEquals(LocationFailureCause.TIMEOUT, lookup.cause)
        assertEquals(500, testScheduler.currentTime)
        val throwing = Fake().apply { fetch = { _, _ -> error("provider error") } }
        assertEquals(LocationFailureCause.TIMEOUT, BestAvailableLocationRepository(throwing).getBestAvailableLocation().cause)
    }
    @Test fun noCacheTimeoutReturnsTimeout() = runTest {
        val source = Fake().apply { fetch = { _, _ -> delay(20_000); null } }
        val lookup = BestAvailableLocationRepository(source) { testScheduler.currentTime }.getBestAvailableLocation(200)
        assertNull(lookup.fix); assertFalse(lookup.fromCache); assertEquals(LocationFailureCause.TIMEOUT, lookup.cause)
    }
    @Test fun timeoutIsReportedEvenWhenInjectedClockDoesNotAdvance() = runTest {
        val source = Fake().apply {
            enabled = setOf(LocationProviderKind.FUSED)
            fetch = { _, _ -> delay(1000); null }
        }
        val lookup = BestAvailableLocationRepository(source) { 0 }.getBestAvailableLocation(200)
        assertNull(lookup.fix)
        assertEquals(LocationFailureCause.TIMEOUT, lookup.cause)
        assertEquals(100, testScheduler.currentTime)
    }
    @Test fun fusedAndGpsConsumeAllocatedBudgetsAndNetworkStillSucceeds() = runTest {
        val source = Fake().apply {
            fetch = { kind, allocated ->
                if (kind == LocationProviderKind.NETWORK) fix()
                else { delay(allocated); null }
            }
        }
        val lookup = BestAvailableLocationRepository(source) { testScheduler.currentTime }.getBestAvailableLocation(8000)
        assertEquals(LocationProviderKind.entries, source.attempts.map { it.first })
        assertEquals(listOf(4000L, 2000L, 2000L), source.attempts.map { it.second })
        assertEquals(LocationSource.NETWORK, lookup.fix!!.source)
        assertNull(lookup.cause)
        assertFalse(lookup.fromCache)
        assertEquals(6000, testScheduler.currentTime)
    }
    @Test fun throwingEnabledProvidersReturnsCleanFailure() = runTest {
        for (failure in listOf(SecurityException("denied"), IllegalStateException("provider failure"))) {
            val source = object : PlatformLocationSource by Fake() {
                override fun enabledProviders(): Set<LocationProviderKind> = throw failure
            }
            val lookup = BestAvailableLocationRepository(source) { 0 }.getBestAvailableLocation()
            assertNull(lookup.fix)
            assertEquals(LocationFailureCause.PROVIDER_DISABLED, lookup.cause)
        }
    }
    @Test fun throwingLastKnownReturnsCleanFailureWhenCurrentAlsoHasNoFix() = runTest {
        for (failure in listOf(SecurityException("denied"), IllegalStateException("cache failure"))) {
            val fake = Fake()
            val source = object : PlatformLocationSource by fake {
                override suspend fun lastKnown(): LocationFix? = throw failure
            }
            val lookup = BestAvailableLocationRepository(source) { 0 }.getBestAvailableLocation()
            assertNull(lookup.fix)
            assertEquals(LocationFailureCause.NO_FIX, lookup.cause)
            assertEquals(LocationProviderKind.entries, fake.attempts.map { it.first })
        }
    }

}
