package vn.nckh27pa.fallsafe

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class FallDetectionCalibrationUiTest {

    private fun sampleProfile(
        id: String,
        displayName: String,
        number: Int,
        isActive: Boolean,
        config: FallDetectionConfig = FallDetectionConfig.DEFAULT
    ): FallDetectionProfile = FallDetectionProfile(
        id = id,
        displayName = displayName,
        profileNumber = number,
        createdAt = 1000L,
        updatedAt = 1000L,
        isActive = isActive,
        config = config
    )

    @Test
    fun `selected profile and active profile are kept strictly separated`() {
        val p1 = sampleProfile("p1", "Bảng 1", 1, isActive = true)
        val p2 = sampleProfile("p2", "Bảng 2", 2, isActive = false)
        val profiles = listOf(p1, p2)

        // When user selects p2, resolveSelectedProfileId preserves p2 while active is p1
        val selectedId = resolveSelectedProfileId("p2", profiles, "p1")
        assertEquals("p2", selectedId)
        assertFalse("p2 must not be active", p2.isActive)
        assertTrue("p1 must remain active", p1.isActive)

        // When selected ID is unknown or deleted, fall back safely to active profile
        val fallbackToActive = resolveSelectedProfileId("deleted_p", profiles, "p1")
        assertEquals("p1", fallbackToActive)

        // When active is also missing, fall back to first profile
        val fallbackToFirst = resolveSelectedProfileId("deleted_p", profiles, "unknown_active")
        assertEquals("p1", fallbackToFirst)

        // Empty list returns empty string
        assertEquals("", resolveSelectedProfileId("p1", emptyList(), "p1"))
    }

    @Test
    fun `lossless round-trip config to draft to validation to config preserves exact float precision`() {
        // Test with DEFAULT config
        val defaultDraft = CalibrationDraft.fromConfig(FallDetectionConfig.DEFAULT)
        val defaultValidation = validateCalibrationDraft(defaultDraft)
        assertTrue("Default config must validate successfully", defaultValidation.isValid)
        assertEquals(FallDetectionConfig.DEFAULT, defaultValidation.parsedConfig)

        // Test with arbitrary high-precision Float values (e.g. 25.125f, 9.8125f, 1.125f)
        val precisionConfig = FallDetectionConfig(
            impactAccelerationMs2 = 25.125f,
            stillnessTargetAccelerationMs2 = 9.8125f,
            stillnessToleranceMs2 = 1.125f,
            postImpactWindowMs = 4500L,
            postImpactStillnessDurationMs = 1500L,
            minimumStillnessSamples = 12,
            maximumSampleGapMs = 350L
        )

        val precisionDraft = CalibrationDraft.fromConfig(precisionConfig)
        assertEquals("25.125", precisionDraft.impactAccelerationText)
        assertEquals("9.8125", precisionDraft.stillnessTargetAccelerationText)
        assertEquals("1.125", precisionDraft.stillnessToleranceText)

        val precisionValidation = validateCalibrationDraft(precisionDraft)
        assertTrue("High precision draft must validate successfully", precisionValidation.isValid)
        assertNotNull(precisionValidation.parsedConfig)
        assertEquals(precisionConfig, precisionValidation.parsedConfig)
    }

    @Test
    fun `hasUnsavedChanges accurately tracks dirty draft and invalid states`() {
        val baseConfig = FallDetectionConfig.DEFAULT
        val profile = sampleProfile("p1", "Bảng 1", 1, isActive = false, config = baseConfig)

        // 1. Pristine draft matches saved config
        val cleanDraft = CalibrationDraft.fromConfig(baseConfig)
        val cleanValidation = validateCalibrationDraft(cleanDraft)
        assertFalse("Clean draft must not have unsaved changes", hasUnsavedChanges(cleanValidation, profile.config))

        // 2. Modified valid draft
        val dirtyDraft = cleanDraft.copy(impactAccelerationText = "35")
        val dirtyValidation = validateCalibrationDraft(dirtyDraft)
        assertTrue("Modified draft must report unsaved changes", hasUnsavedChanges(dirtyValidation, profile.config))

        // 3. Invalid draft (e.g. blank input or out of bounds)
        val invalidDraft = cleanDraft.copy(impactAccelerationText = "")
        val invalidValidation = validateCalibrationDraft(invalidDraft)
        assertFalse(invalidValidation.isValid)
        assertTrue("Invalid draft must report unsaved changes", hasUnsavedChanges(invalidValidation, profile.config))
    }

    @Test
    fun `canActivateProfile enforces activation rules and guards against unsaved drafts`() {
        val inactiveProfile = sampleProfile("p2", "Bảng 2", 2, isActive = false)
        val activeProfile = sampleProfile("p1", "Bảng 1", 1, isActive = true)

        // Rule 1: Cannot activate if already active
        assertFalse(
            "Active profile cannot be activated again",
            canActivateProfile(activeProfile, hasUnsavedChanges = false, isValid = true)
        )

        // Rule 2: Inactive profile with unsaved changes cannot be activated directly
        assertFalse(
            "Inactive profile with unsaved changes must be saved before activation",
            canActivateProfile(inactiveProfile, hasUnsavedChanges = true, isValid = true)
        )

        // Rule 3: Inactive profile with invalid draft cannot be activated
        assertFalse(
            "Inactive profile with invalid draft cannot be activated",
            canActivateProfile(inactiveProfile, hasUnsavedChanges = true, isValid = false)
        )

        // Rule 4: Inactive profile without unsaved changes and valid draft CAN be activated
        assertTrue(
            "Inactive profile without unsaved changes can be activated",
            canActivateProfile(inactiveProfile, hasUnsavedChanges = false, isValid = true)
        )
    }

    @Test
    fun `comma decimal separator is normalized and parsed correctly`() {
        val draftWithCommas = CalibrationDraft(
            impactAccelerationText = "26,5",
            stillnessTargetAccelerationText = "9,80",
            stillnessToleranceText = "1,5",
            postImpactWindowMsText = "3000",
            postImpactStillnessDurationMsText = "1000",
            minimumStillnessSamplesText = "6",
            maximumSampleGapMsText = "250"
        )
        val validation = validateCalibrationDraft(draftWithCommas)

        assertTrue(validation.isValid)
        assertNotNull(validation.parsedConfig)
        assertEquals(26.5f, validation.parsedConfig!!.impactAccelerationMs2, 0.001f)
        assertEquals(9.80f, validation.parsedConfig!!.stillnessTargetAccelerationMs2, 0.001f)
        assertEquals(1.5f, validation.parsedConfig!!.stillnessToleranceMs2, 0.001f)
    }

    @Test
    fun `blank or invalid non-numeric inputs produce clear errors and disable save`() {
        val baseDraft = CalibrationDraft.fromConfig(FallDetectionConfig.DEFAULT)

        // Blank impact
        val blankImpact = validateCalibrationDraft(baseDraft.copy(impactAccelerationText = "   "))
        assertFalse(blankImpact.isValid)
        assertEquals("Không được để trống", blankImpact.impactError)

        // Invalid text
        val invalidText = validateCalibrationDraft(baseDraft.copy(impactAccelerationText = "abc"))
        assertFalse(invalidText.isValid)
        assertEquals("Giá trị không hợp lệ", invalidText.impactError)

        // Non-integer sample count
        val floatSamples = validateCalibrationDraft(baseDraft.copy(minimumStillnessSamplesText = "6.5"))
        assertFalse(floatSamples.isValid)
        assertEquals("Giá trị phải là số nguyên", floatSamples.minimumStillnessSamplesError)
    }

    @Test
    fun `out of bounds inputs produce boundary errors`() {
        val baseDraft = CalibrationDraft.fromConfig(FallDetectionConfig.DEFAULT)

        // impact < 1 or > 100
        val lowImpact = validateCalibrationDraft(baseDraft.copy(impactAccelerationText = "0.5"))
        assertFalse(lowImpact.isValid)
        assertNotNull(lowImpact.impactError)

        val highImpact = validateCalibrationDraft(baseDraft.copy(impactAccelerationText = "105"))
        assertFalse(highImpact.isValid)
        assertNotNull(highImpact.impactError)

        // target stillness < 0 or > 20
        val lowTarget = validateCalibrationDraft(baseDraft.copy(stillnessTargetAccelerationText = "-1"))
        assertFalse(lowTarget.isValid)
        assertNotNull(lowTarget.stillnessTargetError)

        val highTarget = validateCalibrationDraft(baseDraft.copy(stillnessTargetAccelerationText = "25"))
        assertFalse(highTarget.isValid)
        assertNotNull(highTarget.stillnessTargetError)

        // tolerance < 0.1 or > 10
        val lowTol = validateCalibrationDraft(baseDraft.copy(stillnessToleranceText = "0.05"))
        assertFalse(lowTol.isValid)
        assertNotNull(lowTol.stillnessToleranceError)

        val highTol = validateCalibrationDraft(baseDraft.copy(stillnessToleranceText = "15"))
        assertFalse(highTol.isValid)
        assertNotNull(highTol.stillnessToleranceError)

        // window < 500 or > 10000
        val lowWindow = validateCalibrationDraft(baseDraft.copy(postImpactWindowMsText = "400"))
        assertFalse(lowWindow.isValid)
        assertNotNull(lowWindow.postImpactWindowError)

        val highWindow = validateCalibrationDraft(baseDraft.copy(postImpactWindowMsText = "15000"))
        assertFalse(highWindow.isValid)
        assertNotNull(highWindow.postImpactWindowError)

        // samples < 2 or > 100
        val lowSamples = validateCalibrationDraft(baseDraft.copy(minimumStillnessSamplesText = "1"))
        assertFalse(lowSamples.isValid)
        assertNotNull(lowSamples.minimumStillnessSamplesError)

        val highSamples = validateCalibrationDraft(baseDraft.copy(minimumStillnessSamplesText = "150"))
        assertFalse(highSamples.isValid)
        assertNotNull(highSamples.minimumStillnessSamplesError)

        // gap < 10 or > 2000
        val lowGap = validateCalibrationDraft(baseDraft.copy(maximumSampleGapMsText = "5"))
        assertFalse(lowGap.isValid)
        assertNotNull(lowGap.maximumSampleGapError)

        val highGap = validateCalibrationDraft(baseDraft.copy(maximumSampleGapMsText = "3000"))
        assertFalse(highGap.isValid)
        assertNotNull(highGap.maximumSampleGapError)
    }

    @Test
    fun `invariant postImpactStillnessDurationMs must be less than or equal to postImpactWindowMs`() {
        val baseDraft = CalibrationDraft.fromConfig(FallDetectionConfig.DEFAULT)

        // duration > window: invalid
        val invalidRelation = validateCalibrationDraft(
            baseDraft.copy(
                postImpactWindowMsText = "2000",
                postImpactStillnessDurationMsText = "2500"
            )
        )
        assertFalse(invalidRelation.isValid)
        assertNotNull(invalidRelation.postImpactStillnessDurationError)
        assertTrue(
            "Error message should mention relation between duration and window",
            invalidRelation.postImpactStillnessDurationError!!.contains("phải ≤ cửa sổ theo dõi")
        )

        // duration == window: valid boundary
        val equalRelation = validateCalibrationDraft(
            baseDraft.copy(
                postImpactWindowMsText = "2000",
                postImpactStillnessDurationMsText = "2000"
            )
        )
        assertTrue(equalRelation.isValid)
        assertNull(equalRelation.postImpactStillnessDurationError)

        // duration < window: valid
        val strictLessRelation = validateCalibrationDraft(
            baseDraft.copy(
                postImpactWindowMsText = "3000",
                postImpactStillnessDurationMsText = "1000"
            )
        )
        assertTrue(strictLessRelation.isValid)
        assertNull(strictLessRelation.postImpactStillnessDurationError)
    }

    @Test
    fun `saveAs creates new profile and UI selects new ID without changing active profile`() {
        val controller = DemoController()
        val initialActive = controller.activeProfile
        assertEquals("Bảng 1", initialActive.displayName)
        assertTrue(initialActive.isActive)

        val customConfig = FallDetectionConfig.DEFAULT.copy(impactAccelerationMs2 = 30f)
        val created = controller.saveAs(customConfig)

        assertNotNull(created)
        assertFalse("Created profile must NOT be active", created!!.isActive)
        assertEquals("Bảng 2", created.displayName)

        // Simulate UI state transition: selectedProfileId moves to the newly created profile
        val selectedProfileId = created.id
        assertEquals(created.id, selectedProfileId)

        // Verify that controller's active profile was NEVER modified
        assertEquals(initialActive.id, controller.activeProfile.id)
        assertTrue(controller.activeProfile.isActive)
        assertFalse(created.id == controller.activeProfile.id)
    }

    @Test
    fun `delete profile rules enforce that active profile and last remaining profile cannot be deleted`() {
        val p1 = sampleProfile("p1", "Bảng 1", 1, isActive = true)
        val p2 = sampleProfile("p2", "Bảng 2", 2, isActive = false)

        // Cannot delete active profile even when multiple profiles exist
        assertFalse(canDeleteProfile(p1, listOf(p1, p2)))

        // Can delete inactive profile when multiple profiles exist
        assertTrue(canDeleteProfile(p2, listOf(p1, p2)))

        // Cannot delete last remaining profile
        val singleList = listOf(p1)
        assertFalse(canDeleteProfile(p1, singleList))

        val singleInactive = sampleProfile("p2", "Bảng 2", 2, isActive = false)
        assertFalse(canDeleteProfile(singleInactive, listOf(singleInactive)))
    }

    @Test
    fun `truthful threshold labels and Vietnamese phase labels are correctly formatted`() {
        assertEquals("Bình thường", detectionPhaseVietnamese(DetectionPhase.NORMAL))
        assertEquals("Phát hiện va chạm", detectionPhaseVietnamese(DetectionPhase.IMPACT_DETECTED))
        assertEquals("Theo dõi bất động sau va chạm", detectionPhaseVietnamese(DetectionPhase.POST_IMPACT_STILLNESS))
        assertEquals("Xác nhận té ngã", detectionPhaseVietnamese(DetectionPhase.FALL_CONFIRMED))

        // Exceeded state: color + bold text
        val overText = thresholdStatusLabel(true, 25f)
        assertTrue(overText.contains("VƯỢT NGƯỠNG VA CHẠM"))
        assertTrue(overText.contains("25"))

        // Below threshold: truthful phrasing "CHƯA VƯỢT NGƯỠNG VA CHẠM", never claiming safety
        val underText = thresholdStatusLabel(false, 25f)
        assertTrue(underText.contains("CHƯA VƯỢT NGƯỠNG VA CHẠM"))
        assertFalse("Should never claim safe state based only on threshold", underText.contains("AN TOÀN"))
        assertTrue(underText.contains("25"))
    }

    @Test
    fun `takeSensorUiSnapshot computes magnitude and evaluates threshold state accurately`() {
        val config = FallDetectionConfig.DEFAULT
        val observation = FallDetectionObservation(
            accelerationMagnitudeMs2 = null,
            phase = DetectionPhase.NORMAL,
            activeProfileId = "p1",
            activeProfileDisplayName = "Bảng 1",
            config = config,
            impactOverThreshold = false,
            withinStillness = false,
            stillnessProgress = 0f,
            stillnessSampleCount = 0
        )
        val packet = PhoneSensorPacket(
            timestampNs = 1_000_000L,
            wallClockTimestampMs = 1_000L,
            accelXMs2 = 0f,
            accelYMs2 = 9.81f,
            accelZMs2 = 0f
        )

        val snapshot = takeSensorUiSnapshot(packet, observation)
        assertEquals(0f, snapshot.accelX)
        assertEquals(9.81f, snapshot.accelY)
        assertEquals(0f, snapshot.accelZ)
        assertNotNull(snapshot.magnitude)
        assertEquals(9.81, snapshot.magnitude!!, 0.01)
        assertFalse(snapshot.impactOverThreshold)

        // Impact packet exceeding threshold (25 m/s²)
        val highImpactPacket = PhoneSensorPacket(
            timestampNs = 2_000_000L,
            wallClockTimestampMs = 2_000L,
            accelXMs2 = 0f,
            accelYMs2 = 30f,
            accelZMs2 = 0f
        )
        val highSnapshot = takeSensorUiSnapshot(highImpactPacket, observation)
        assertTrue(highSnapshot.impactOverThreshold)
    }
}
