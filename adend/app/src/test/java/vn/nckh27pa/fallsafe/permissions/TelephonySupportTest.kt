package vn.nckh27pa.fallsafe.permissions

import org.junit.Assert.*
import org.junit.Test

class TelephonySupportTest {
    @Test fun devicePublishingOnlyBaseTelephonyIsSupported() {
        // Xiaomi 2201117SG / Android 13 / MIUI V816: android.hardware.telephony=true, .calling=false, .messaging=false
        assertTrue(resolveTelephonySupport(hasBaseTelephony = true, hasGranularFeature = false))
    }
    @Test fun devicePublishingGranularFeatureIsSupported() {
        assertTrue(resolveTelephonySupport(hasBaseTelephony = false, hasGranularFeature = true))
    }
    @Test fun deviceWithNoTelephonyIsUnsupported() {
        assertFalse(resolveTelephonySupport(hasBaseTelephony = false, hasGranularFeature = false))
    }
}
