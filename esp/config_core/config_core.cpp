#include "config_core.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace fallsafe::config {
namespace {

ValidationResult invalid(const char* field) { return {ErrorCode::ValidationError, field}; }
bool finiteScaled(double value, double minimum, double maximum, double scale) noexcept {
    if (!std::isfinite(value) || value < minimum || value > maximum) return false;
    return std::abs(value * scale - std::round(value * scale)) <= 1e-8;
}
bool inRange(std::uint64_t value, std::uint64_t minimum, std::uint64_t maximum) noexcept {
    return value >= minimum && value <= maximum;
}
bool isHex(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool validHash(const std::string& hash) noexcept {
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c) {
        return c >= '0' && c <= '9' ? true : c >= 'a' && c <= 'f';
    });
}
std::uint32_t rotateRight(std::uint32_t value, unsigned shift) noexcept {
    return (value >> shift) | (value << (32U - shift));
}
std::string semanticHash(const ApplyRequest& request) {
    return sha256Hex(std::to_string(request.expectedConfigRevision) + "|" + request.configHash + "|" +
                     canonicalDocument(request.profileId, request.profileRevision, request.config));
}
bool active(AlertState state) noexcept {
    return state == AlertState::Suspected || state == AlertState::Verifying ||
           state == AlertState::LocalAlerting;
}

std::string recordPayload(const AppliedRecord& record) {
    std::ostringstream out;
    out << 1 << ' ' << std::quoted(record.profileId) << ' ' << record.profileRevision << ' '
        << record.configRevision << ' ' << std::quoted(record.configHash) << ' '
        << std::quoted(record.committingRequestId) << ' '
        << std::quoted(record.committingSemanticHash) << ' '
        << record.committingResult.changed << ' '
        << std::setprecision(17)
        << record.config.impactAccelerationMs2 << ' '
        << record.config.stillnessTargetAccelerationMs2 << ' '
        << record.config.stillnessToleranceMs2 << ' '
        << record.config.postImpactWindowMs << ' '
        << record.config.postImpactStillnessDurationMs << ' '
        << record.config.minimumStillnessSamples << ' '
        << record.config.maximumSampleGapMs << ' '
        << record.config.pressureEvidenceEnabled << ' '
        << record.config.pressureMinimumRisePa << ' '
        << record.config.pressureWindowMs << ' '
        << record.config.pressureMinimumSamples << ' '
        << record.config.pressureFilterAlpha << ' '
        << record.config.pressureStaleAfterMs;
    return out.str();
}
std::optional<AppliedRecord> parseRecordFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::string checksum, payload;
    if (!std::getline(file, checksum) || !std::getline(file, payload) || sha256Hex(payload) != checksum)
        return std::nullopt;
    std::istringstream in(payload);
    int version = 0;
    AppliedRecord record;
    bool changed = false;
    if (!(in >> version >> std::quoted(record.profileId) >> record.profileRevision >> record.configRevision
          >> std::quoted(record.configHash) >> std::quoted(record.committingRequestId)
          >> std::quoted(record.committingSemanticHash) >> changed
          >> record.config.impactAccelerationMs2 >> record.config.stillnessTargetAccelerationMs2
          >> record.config.stillnessToleranceMs2 >> record.config.postImpactWindowMs
          >> record.config.postImpactStillnessDurationMs >> record.config.minimumStillnessSamples
          >> record.config.maximumSampleGapMs >> record.config.pressureEvidenceEnabled
          >> record.config.pressureMinimumRisePa >> record.config.pressureWindowMs
          >> record.config.pressureMinimumSamples >> record.config.pressureFilterAlpha
          >> record.config.pressureStaleAfterMs) || version != 1) return std::nullopt;
    in >> std::ws;
    if (!in.eof() || !validUuid(record.profileId) || record.profileRevision == 0 ||
        record.configRevision == 0 || !validHash(record.configHash) || !validate(record.config).ok() ||
        canonicalHash(record.profileId, record.profileRevision, record.config) != record.configHash ||
        !validUuid(record.committingRequestId) || !validHash(record.committingSemanticHash)) return std::nullopt;
    record.committingResult = {true, changed, ErrorCode::None, record.configRevision, record.configHash};
    return record;
}

} // namespace

ValidationResult validate(const Config& c) noexcept {
    if (!finiteScaled(c.impactAccelerationMs2, 1, 100, 1000)) return invalid("config.impactAccelerationMs2");
    if (!finiteScaled(c.stillnessTargetAccelerationMs2, 0, 20, 1000)) return invalid("config.stillnessTargetAccelerationMs2");
    if (!finiteScaled(c.stillnessToleranceMs2, .1, 10, 1000)) return invalid("config.stillnessToleranceMs2");
    if (!inRange(c.postImpactWindowMs, 500, 10000)) return invalid("config.postImpactWindowMs");
    if (!inRange(c.postImpactStillnessDurationMs, 100, 10000)) return invalid("config.postImpactStillnessDurationMs");
    if (!inRange(c.minimumStillnessSamples, 2, 100)) return invalid("config.minimumStillnessSamples");
    if (!inRange(c.maximumSampleGapMs, 10, 2000)) return invalid("config.maximumSampleGapMs");
    if (!finiteScaled(c.pressureMinimumRisePa, 1, 200, 100)) return invalid("config.pressureMinimumRisePa");
    if (!inRange(c.pressureWindowMs, 500, 10000)) return invalid("config.pressureWindowMs");
    if (!inRange(c.pressureMinimumSamples, 2, 100)) return invalid("config.pressureMinimumSamples");
    if (!finiteScaled(c.pressureFilterAlpha, .01, 1, 1000)) return invalid("config.pressureFilterAlpha");
    if (!inRange(c.pressureStaleAfterMs, 100, 5000)) return invalid("config.pressureStaleAfterMs");
    if (c.postImpactStillnessDurationMs > c.postImpactWindowMs)
        return invalid("config.postImpactStillnessDurationMs");
    if (c.pressureStaleAfterMs > c.pressureWindowMs) return invalid("config.pressureStaleAfterMs");
    return {};
}

bool validUuid(const std::string& text) noexcept {
    if (text.size() != 36 || text[8] != '-' || text[13] != '-' || text[18] != '-' || text[23] != '-')
        return false;
    for (std::size_t i = 0; i < text.size(); ++i)
        if (i != 8 && i != 13 && i != 18 && i != 23 && !isHex(text[i])) return false;
    return true;
}

std::string canonicalDocument(const std::string& profileId, std::uint64_t revision, const Config& c) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schemaVersion\":1,\"profileTarget\":\"ESP32\",\"profileId\":\"" << profileId
        << "\",\"profileRevision\":" << revision << ",\"config\":{" << std::fixed << std::setprecision(3)
        << "\"impactAccelerationMs2\":" << c.impactAccelerationMs2
        << ",\"stillnessTargetAccelerationMs2\":" << c.stillnessTargetAccelerationMs2
        << ",\"stillnessToleranceMs2\":" << c.stillnessToleranceMs2 << std::setprecision(0)
        << ",\"postImpactWindowMs\":" << c.postImpactWindowMs
        << ",\"postImpactStillnessDurationMs\":" << c.postImpactStillnessDurationMs
        << ",\"minimumStillnessSamples\":" << c.minimumStillnessSamples
        << ",\"maximumSampleGapMs\":" << c.maximumSampleGapMs
        << ",\"pressureEvidenceEnabled\":" << (c.pressureEvidenceEnabled ? "true" : "false")
        << std::setprecision(2) << ",\"pressureMinimumRisePa\":" << c.pressureMinimumRisePa
        << std::setprecision(0) << ",\"pressureWindowMs\":" << c.pressureWindowMs
        << ",\"pressureMinimumSamples\":" << c.pressureMinimumSamples
        << std::setprecision(3) << ",\"pressureFilterAlpha\":" << c.pressureFilterAlpha
        << std::setprecision(0) << ",\"pressureStaleAfterMs\":" << c.pressureStaleAfterMs << "}}";
    return out.str();
}

std::string sha256Hex(const std::string& bytes) {
    static constexpr std::array<std::uint32_t, 64> constants = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
    std::vector<std::uint8_t> message(bytes.begin(), bytes.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(0x80U);
    while ((message.size() % 64U) != 56U) message.push_back(0U);
    for (int shift = 56; shift >= 0; shift -= 8) message.push_back(static_cast<std::uint8_t>(bitLength >> shift));
    std::array<std::uint32_t, 8> state = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
                                          0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t p = offset + i * 4;
            words[i] = (static_cast<std::uint32_t>(message[p]) << 24U) |
                       (static_cast<std::uint32_t>(message[p + 1]) << 16U) |
                       (static_cast<std::uint32_t>(message[p + 2]) << 8U) | message[p + 3];
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const auto s0 = rotateRight(words[i-15],7) ^ rotateRight(words[i-15],18) ^ (words[i-15] >> 3U);
            const auto s1 = rotateRight(words[i-2],17) ^ rotateRight(words[i-2],19) ^ (words[i-2] >> 10U);
            words[i] = words[i-16] + s0 + words[i-7] + s1;
        }
        auto a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for (std::size_t i = 0; i < 64; ++i) {
            const auto sum1=rotateRight(e,6)^rotateRight(e,11)^rotateRight(e,25);
            const auto choice=(e&f)^((~e)&g);
            const auto temp1=h+sum1+choice+constants[i]+words[i];
            const auto sum0=rotateRight(a,2)^rotateRight(a,13)^rotateRight(a,22);
            const auto majority=(a&b)^(a&c)^(b&c);
            const auto temp2=sum0+majority;
            h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto value : state) out << std::setw(8) << value;
    return out.str();
}

std::string canonicalHash(const std::string& profileId, std::uint64_t revision, const Config& config) {
    return sha256Hex(canonicalDocument(profileId, revision, config));
}
bool equalConfig(const Config& a, const Config& b) noexcept {
    return a.impactAccelerationMs2==b.impactAccelerationMs2 && a.stillnessTargetAccelerationMs2==b.stillnessTargetAccelerationMs2 &&
        a.stillnessToleranceMs2==b.stillnessToleranceMs2 && a.postImpactWindowMs==b.postImpactWindowMs &&
        a.postImpactStillnessDurationMs==b.postImpactStillnessDurationMs && a.minimumStillnessSamples==b.minimumStillnessSamples &&
        a.maximumSampleGapMs==b.maximumSampleGapMs && a.pressureEvidenceEnabled==b.pressureEvidenceEnabled &&
        a.pressureMinimumRisePa==b.pressureMinimumRisePa && a.pressureWindowMs==b.pressureWindowMs &&
        a.pressureMinimumSamples==b.pressureMinimumSamples && a.pressureFilterAlpha==b.pressureFilterAlpha &&
        a.pressureStaleAfterMs==b.pressureStaleAfterMs;
}

void MotionDetector::reset() noexcept {
    observation_ = {};
    hasTimestamp_ = false;
    impactAt_ = lastAt_ = stillnessAt_ = 0;
}
bool MotionDetector::accept(std::uint64_t at, double x, double y, double z) noexcept {
    const double magnitude = std::sqrt(x*x+y*y+z*z);
    if (!std::isfinite(magnitude)) { reset(); return false; }
    if (hasTimestamp_ && (at <= lastAt_ || at - lastAt_ > config_.maximumSampleGapMs)) reset();
    hasTimestamp_ = true; lastAt_ = at; observation_.accelerationMagnitudeMs2 = magnitude;
    if (observation_.phase == DetectionPhase::FallConfirmed) return true;
    if (observation_.phase == DetectionPhase::Normal) {
        if (magnitude >= config_.impactAccelerationMs2) {
            impactAt_ = at;
            observation_.phase = DetectionPhase::ImpactDetected;
        }
        return false;
    }
    if (at - impactAt_ > config_.postImpactWindowMs) { reset(); return false; }
    const bool still = std::abs(magnitude-config_.stillnessTargetAccelerationMs2) <= config_.stillnessToleranceMs2;
    if (!still) {
        observation_.phase = DetectionPhase::ImpactDetected;
        observation_.stillnessSamples = 0;
        observation_.stillnessDurationMs = 0;
        return false;
    }
    if (observation_.phase != DetectionPhase::PostImpactStillness) {
        observation_.phase = DetectionPhase::PostImpactStillness;
        stillnessAt_ = at;
        observation_.stillnessSamples = 1;
    } else ++observation_.stillnessSamples;
    observation_.stillnessDurationMs = at - stillnessAt_;
    if (observation_.stillnessSamples >= config_.minimumStillnessSamples &&
        observation_.stillnessDurationMs >= config_.postImpactStillnessDurationMs) {
        observation_.phase = DetectionPhase::FallConfirmed;
        return true;
    }
    return false;
}

PressureCore::PressureCore(Config config, bool present)
    : config_(config), present_(present), status_(present ? SensorStatus::Calibrating : SensorStatus::Unavailable) {}
bool PressureCore::replaceReferenceFromStableSamples(const std::vector<double>& raw) {
    if (!present_ || raw.size() < config_.pressureMinimumSamples) return false;
    if (std::any_of(raw.begin(), raw.end(), [](double v){ return !std::isfinite(v) || v <= 0; })) return false;
    const auto bounds = std::minmax_element(raw.begin(), raw.end());
    if (*bounds.second - *bounds.first > 10.0) return false;
    double sum = 0; for (double value : raw) sum += value;
    referencePa_ = sum / static_cast<double>(raw.size());
    status_ = SensorStatus::Ok;
    return true;
}
bool PressureCore::bootstrap(const std::vector<double>& raw, std::uint64_t) {
    if (!replaceReferenceFromStableSamples(raw)) return false;
    ema_.reset(); samples_.clear();
    return true;
}
void PressureCore::sample(std::uint64_t at, double raw) noexcept {
    if (!present_ || !std::isfinite(raw) || raw <= 0 || (!samples_.empty() && at <= samples_.back().at)) return;
    ema_ = ema_ ? config_.pressureFilterAlpha*raw+(1-config_.pressureFilterAlpha)*(*ema_) : raw;
    samples_.push_back({at,*ema_});
    while (!samples_.empty() && (at-samples_.front().at > config_.pressureWindowMs || samples_.size()>128))
        samples_.erase(samples_.begin());
    status_ = referencePa_ ? SensorStatus::Ok : SensorStatus::Calibrating;
}
void PressureCore::setError() noexcept { if (present_) status_ = SensorStatus::Error; }
PressureObservation PressureCore::observe(std::uint64_t now) const noexcept {
    PressureObservation out; out.status=status_; out.referencePa=referencePa_;
    if (!present_) { out.status=SensorStatus::Unavailable; out.referencePa.reset(); return out; }
    if (samples_.empty() || status_==SensorStatus::Error) return out;
    if (now < samples_.back().at || now-samples_.back().at > config_.pressureStaleAfterMs) {
        out.status=SensorStatus::Stale;
        return out;
    }
    if (status_ != SensorStatus::Ok || !referencePa_) return out;
    out.currentPa=samples_.back().filtered;
    out.deltaPa=*out.currentPa-*referencePa_;
    out.windowDeltaPa=samples_.back().filtered-samples_.front().filtered;
    out.altitudeDeltaM=44330.0*(1.0-std::pow(*out.currentPa / *referencePa_,1.0/5.255));
    out.sampleAgeMs=now-samples_.back().at;
    out.sampleCount=samples_.size();
    out.corroborated=config_.pressureEvidenceEnabled && samples_.size()>=config_.pressureMinimumSamples &&
                       *out.windowDeltaPa>=config_.pressureMinimumRisePa;
    return out;
}

FileDurableStore::FileDurableStore(std::string basePath) : basePath_(std::move(basePath)) {}
std::optional<AppliedRecord> FileDurableStore::load() const {
    const auto a=parseRecordFile(basePath_+".a"), b=parseRecordFile(basePath_+".b");
    if (!a) return b;
    if (!b) return a;
    return a->configRevision >= b->configRevision ? a : b;
}
bool FileDurableStore::save(const AppliedRecord& record) {
    if (failNextWrite_) { failNextWrite_=false; return false; }
    const auto a=parseRecordFile(basePath_+".a"), b=parseRecordFile(basePath_+".b");
    std::string target;
    if (!a) target=basePath_+".a";
    else if (!b) target=basePath_+".b";
    else target=a->configRevision<=b->configRevision ? basePath_+".a" : basePath_+".b";
    const auto temporary=target+".tmp", payload=recordPayload(record);
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if (!out || !(out << sha256Hex(payload) << '\n' << payload << '\n')) return false;
        out.flush(); if (!out) return false;
    }
    if (std::rename(temporary.c_str(),target.c_str()) != 0) { std::remove(temporary.c_str()); return false; }
    ++writeCount_;
    return true;
}
void FileDurableStore::corruptNewestForTest() {
    const auto a=parseRecordFile(basePath_+".a"), b=parseRecordFile(basePath_+".b");
    std::string target;
    if (a && b) target=a->configRevision>=b->configRevision ? basePath_+".a" : basePath_+".b";
    else if (a) target=basePath_+".a"; else if (b) target=basePath_+".b"; else return;
    std::ofstream out(target,std::ios::binary|std::ios::trunc); out << "corrupt\n";
}

ApplyService::ApplyService(FileDurableStore& store, bool barometer, std::function<void()> reset)
    : store_(store), barometerAvailable_(barometer), resetEvidence_(std::move(reset)), current_(store.load()) {
    if (current_) cache_.push_back({current_->committingRequestId,current_->committingSemanticHash,current_->committingResult});
}
ApplyResult ApplyService::apply(const ApplyRequest& request, AlertState state) {
    const auto semantics=semanticHash(request);
    const auto cached=std::find_if(cache_.begin(),cache_.end(),[&](const CacheEntry& e){return e.requestId==request.requestId;});
    if (cached!=cache_.end()) return cached->semanticHash==semantics ? cached->result : ApplyResult{false,false,ErrorCode::RequestIdReused,0,{}};
    if (active(state)) return {false,false,ErrorCode::BusyAlertActive,0,{}};
    if (!validUuid(request.requestId) || !validUuid(request.profileId) || request.profileRevision==0 || !validate(request.config).ok())
        return {false,false,ErrorCode::ValidationError,0,{}};
    if (canonicalHash(request.profileId,request.profileRevision,request.config)!=request.configHash)
        return {false,false,ErrorCode::HashMismatch,0,{}};
    if (request.config.pressureEvidenceEnabled && !barometerAvailable_)
        return {false,false,ErrorCode::SensorUnavailable,0,{}};
    const std::uint64_t currentRevision=current_ ? current_->configRevision : 0;
    if (request.expectedConfigRevision!=currentRevision) return {false,false,ErrorCode::RevisionConflict,currentRevision,{}};
    if (current_ && current_->configHash==request.configHash) {
        ApplyResult result{true,false,ErrorCode::None,currentRevision,current_->configHash};
        cache_.push_back({request.requestId,semantics,result});
        if (cache_.size()>16) cache_.erase(cache_.begin());
        return result;
    }
    ApplyResult result{true,true,ErrorCode::None,currentRevision+1,request.configHash};
    AppliedRecord candidate{request.profileId,request.profileRevision,currentRevision+1,request.configHash,request.config,
                            request.requestId,semantics,result};
    if (!store_.save(candidate)) return {false,false,ErrorCode::StorageError,currentRevision,{}};
    current_=candidate;
    resetEvidence_();
    cache_.push_back({request.requestId,semantics,result});
    if (cache_.size()>16) cache_.erase(cache_.begin());
    return result;
}

} // namespace fallsafe::config
