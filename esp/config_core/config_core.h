#ifndef FALLSAFE_CONFIG_CORE_H
#define FALLSAFE_CONFIG_CORE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fallsafe::config {

struct Config {
    double impactAccelerationMs2 = 25.000;
    double stillnessTargetAccelerationMs2 = 9.810;
    double stillnessToleranceMs2 = 1.000;
    std::uint64_t postImpactWindowMs = 3000;
    std::uint64_t postImpactStillnessDurationMs = 1000;
    std::uint32_t minimumStillnessSamples = 6;
    std::uint64_t maximumSampleGapMs = 250;
    bool pressureEvidenceEnabled = false;
    double pressureMinimumRisePa = 12.00;
    std::uint64_t pressureWindowMs = 3000;
    std::uint32_t pressureMinimumSamples = 5;
    double pressureFilterAlpha = 0.200;
    std::uint64_t pressureStaleAfterMs = 1000;
};

enum class ErrorCode {
    None, ValidationError, HashMismatch, RevisionConflict, RequestIdReused,
    BusyAlertActive, SensorUnavailable, StorageError
};
struct ValidationResult {
    ErrorCode error = ErrorCode::None;
    std::string field;
    bool ok() const noexcept { return error == ErrorCode::None; }
};
ValidationResult validate(const Config& config) noexcept;
bool validUuid(const std::string& text) noexcept;
std::string canonicalDocument(const std::string& profileId, std::uint64_t profileRevision,
                              const Config& config);
std::string canonicalHash(const std::string& profileId, std::uint64_t profileRevision,
                          const Config& config);
std::string sha256Hex(const std::string& bytes);
bool equalConfig(const Config& lhs, const Config& rhs) noexcept;

enum class DetectionPhase { Normal, ImpactDetected, PostImpactStillness, FallConfirmed };
struct MotionObservation {
    DetectionPhase phase = DetectionPhase::Normal;
    double accelerationMagnitudeMs2 = 0;
    std::uint32_t stillnessSamples = 0;
    std::uint64_t stillnessDurationMs = 0;
};
class MotionDetector {
public:
    explicit MotionDetector(Config config) : config_(config) {}
    bool accept(std::uint64_t timestampMs, double xMs2, double yMs2, double zMs2) noexcept;
    void reset() noexcept;
    const MotionObservation& observation() const noexcept { return observation_; }
private:
    Config config_;
    MotionObservation observation_{};
    bool hasTimestamp_ = false;
    std::uint64_t impactAt_ = 0, lastAt_ = 0, stillnessAt_ = 0;
};

enum class SensorStatus { Ok, Calibrating, Stale, Unavailable, Error };
struct PressureObservation {
    SensorStatus status = SensorStatus::Unavailable;
    std::optional<double> currentPa, referencePa, deltaPa, windowDeltaPa, altitudeDeltaM;
    std::optional<std::uint64_t> sampleAgeMs;
    std::optional<std::size_t> sampleCount;
    std::optional<bool> corroborated;
};
class PressureCore {
public:
    PressureCore(Config config, bool present);
    bool bootstrap(const std::vector<double>& rawSamples, std::uint64_t timestampMs);
    bool replaceReferenceFromStableSamples(const std::vector<double>& rawSamples);
    void sample(std::uint64_t timestampMs, double rawPa) noexcept;
    void setError() noexcept;
    PressureObservation observe(std::uint64_t nowMs) const noexcept;
    SensorStatus status() const noexcept { return status_; }
    std::optional<double> referencePa() const noexcept { return referencePa_; }
private:
    struct Sample { std::uint64_t at; double filtered; };
    Config config_;
    bool present_;
    SensorStatus status_;
    std::optional<double> referencePa_, ema_;
    std::vector<Sample> samples_;
};

enum class AlertState { Monitoring, Suspected, Verifying, LocalAlerting, Degraded };
struct ApplyRequest {
    std::string requestId;
    std::string profileId;
    std::uint64_t profileRevision = 0;
    std::uint64_t expectedConfigRevision = 0;
    std::string configHash;
    Config config;
};
struct ApplyResult {
    bool ok = false;
    bool changed = false;
    ErrorCode error = ErrorCode::None;
    std::uint64_t configRevision = 0;
    std::string configHash;
};
struct AppliedRecord {
    std::string profileId;
    std::uint64_t profileRevision = 0;
    std::uint64_t configRevision = 0;
    std::string configHash;
    Config config;
    std::string committingRequestId;
    std::string committingSemanticHash;
    ApplyResult committingResult;
};
class FileDurableStore {
public:
    explicit FileDurableStore(std::string basePath);
    std::optional<AppliedRecord> load() const;
    bool save(const AppliedRecord& record);
    std::size_t writeCount() const noexcept { return writeCount_; }
    void corruptNewestForTest();
    void failNextWriteForTest() noexcept { failNextWrite_ = true; }
private:
    std::string basePath_;
    mutable std::size_t writeCount_ = 0;
    bool failNextWrite_ = false;
};
class ApplyService {
public:
    ApplyService(FileDurableStore& store, bool barometerAvailable,
                 std::function<void()> resetEvidence);
    ApplyResult apply(const ApplyRequest& request, AlertState state);
    std::optional<AppliedRecord> current() const { return current_; }
private:
    struct CacheEntry { std::string requestId, semanticHash; ApplyResult result; };
    FileDurableStore& store_;
    bool barometerAvailable_;
    std::function<void()> resetEvidence_;
    std::optional<AppliedRecord> current_;
    std::vector<CacheEntry> cache_;
};

} // namespace fallsafe::config
#endif
