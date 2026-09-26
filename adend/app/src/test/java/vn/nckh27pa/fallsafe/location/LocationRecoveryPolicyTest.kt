package vn.nckh27pa.fallsafe.location

import org.junit.Assert.assertEquals
import org.junit.Test

class LocationRecoveryPolicyTest {
    private fun input(
        signal: LocationSignal = LocationSignal.FOREGROUND_RESUMED,
        permission: LocationPermission = LocationPermission.PRECISE,
        providers: Set<LocationProviderKind> = LocationProviderKind.entries.toSet(),
        changed: Boolean = true,
        inFlight: Boolean = false,
        hasFix: Boolean = false,
        fixIsFresh: Boolean = false
    ) = LocationRecoveryInput(
        signal = signal,
        availability = LocationAvailability(permission, providers),
        availabilityChanged = changed,
        requestInFlight = inFlight,
        hasFix = hasFix,
        fixIsFresh = fixIsFresh
    )

    @Test fun requestInFlightAlwaysReturnsNone() {
        val policy = LocationRecoveryPolicy()
        assertEquals(
            LocationRecoveryDecision.None,
            policy.decide(input(inFlight = true, changed = true, hasFix = false, fixIsFresh = false))
        )
    }

    @Test fun deniedPermissionWithNoFixReturnsPermissionDenied() {
        val policy = LocationRecoveryPolicy()
        assertEquals(
            LocationRecoveryDecision.PermissionDenied,
            policy.decide(input(permission = LocationPermission.DENIED, changed = true))
        )
    }

    @Test fun deniedPermissionWithFixReturnsNone() {
        val policy = LocationRecoveryPolicy()
        assertEquals(
            LocationRecoveryDecision.None,
            policy.decide(input(permission = LocationPermission.DENIED, changed = true, hasFix = true))
        )
    }

    @Test fun emptyProvidersWithNoFixReturnsProviderDisabled() {
        val policy = LocationRecoveryPolicy()
        assertEquals(
            LocationRecoveryDecision.ProviderDisabled,
            policy.decide(input(providers = emptySet(), changed = true))
        )
    }

    @Test fun emptyProvidersWithFixReturnsNone() {
        val policy = LocationRecoveryPolicy()
        assertEquals(
            LocationRecoveryDecision.None,
            policy.decide(input(providers = emptySet(), changed = true, hasFix = true))
        )
    }

    @Test fun usableWithAvailabilityChangedReturnsAcquire() {
        val policy = LocationRecoveryPolicy()
        assertEquals(LocationRecoveryDecision.Acquire, policy.decide(input(changed = true)))
    }

    @Test fun usableWithoutFixNeedsRecordedAttemptBeforeAcquiring() {
        var now = 0L
        val policy = LocationRecoveryPolicy({ now })
        assertEquals(LocationRecoveryDecision.None, policy.decide(input(changed = false, hasFix = false, fixIsFresh = false)))
    }

    @Test fun usableWithoutFixRespectsCooldown() {
        var now = 0L
        val policy = LocationRecoveryPolicy({ now })
        assertEquals(LocationRecoveryDecision.Acquire, policy.decide(input(changed = true)))
        now = 5_000L
        assertEquals(LocationRecoveryDecision.None, policy.decide(input(changed = false, hasFix = false, fixIsFresh = false)))
        now = 20_000L
        assertEquals(LocationRecoveryDecision.Acquire, policy.decide(input(changed = false, hasFix = false, fixIsFresh = false)))
    }

    @Test fun usableWithFreshFixAndNoChangeReturnsNone() {
        val policy = LocationRecoveryPolicy()
        assertEquals(
            LocationRecoveryDecision.None,
            policy.decide(input(changed = false, hasFix = true, fixIsFresh = true))
        )
    }

    @Test fun staleFixWithoutChangeAcquiresAfterCooldown() {
        var now = 0L
        val policy = LocationRecoveryPolicy({ now })
        assertEquals(LocationRecoveryDecision.Acquire, policy.decide(input(changed = true)))
        now = 30_000L
        assertEquals(LocationRecoveryDecision.Acquire, policy.decide(input(changed = false, hasFix = true, fixIsFresh = false)))
    }

    @Test fun markAttemptAndResetManageCooldownBookkeeping() {
        var now = 0L
        val policy = LocationRecoveryPolicy({ now })
        policy.markAttempt()
        now = 5_000L
        assertEquals(LocationRecoveryDecision.None, policy.decide(input(changed = false, hasFix = false, fixIsFresh = false)))
        policy.reset()
        now = 6_000L
        assertEquals(LocationRecoveryDecision.None, policy.decide(input(changed = false, hasFix = false, fixIsFresh = false)))
        now = 100_000L
        assertEquals(LocationRecoveryDecision.None, policy.decide(input(changed = false, hasFix = false, fixIsFresh = false)))
    }
}
