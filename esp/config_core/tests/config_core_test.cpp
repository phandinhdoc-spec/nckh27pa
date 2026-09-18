#include "config_core.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
using namespace fallsafe::config;

static ApplyRequest request(const char* rid, const Config& c = Config{}, std::uint64_t expected = 0) {
    ApplyRequest r;
    r.requestId = rid;
    r.profileId = "4ce38b32-baa5-4f3f-99be-8a920bd2d17d";
    r.profileRevision = 3;
    r.expectedConfigRevision = expected;
    r.config = c;
    r.configHash = canonicalHash(r.profileId, r.profileRevision, r.config);
    return r;
}
static void test_hash_and_validation() {
    Config c;
    assert(validate(c).ok());
    assert(canonicalDocument("4ce38b32-baa5-4f3f-99be-8a920bd2d17d", 3, c) ==
      "{\"schemaVersion\":1,\"profileTarget\":\"ESP32\",\"profileId\":\"4ce38b32-baa5-4f3f-99be-8a920bd2d17d\",\"profileRevision\":3,\"config\":{\"impactAccelerationMs2\":25.000,\"stillnessTargetAccelerationMs2\":9.810,\"stillnessToleranceMs2\":1.000,\"postImpactWindowMs\":3000,\"postImpactStillnessDurationMs\":1000,\"minimumStillnessSamples\":6,\"maximumSampleGapMs\":250,\"pressureEvidenceEnabled\":false,\"pressureMinimumRisePa\":12.00,\"pressureWindowMs\":3000,\"pressureMinimumSamples\":5,\"pressureFilterAlpha\":0.200,\"pressureStaleAfterMs\":1000}}");
    assert(canonicalHash("4ce38b32-baa5-4f3f-99be-8a920bd2d17d", 3, c) == "88dcc136a42afb992df14823614f9f9f4459e4093ade82be7e5624dd5a4b11fe");
    c.postImpactStillnessDurationMs = 3001; assert(!validate(c).ok());
    c = {}; c.pressureStaleAfterMs = 3001; assert(!validate(c).ok());
    c = {}; c.pressureFilterAlpha = std::nan(""); assert(!validate(c).ok());
    c = {}; c.impactAccelerationMs2 = 25.0001; assert(!validate(c).ok());
    c = {}; c.pressureMinimumRisePa = 12.001; assert(!validate(c).ok());
    c = {}; c.postImpactWindowMs = 499; assert(!validate(c).ok());
}
static void test_motion_uses_all_parameters() {
    Config c; c.postImpactStillnessDurationMs=200; c.minimumStillnessSamples=3; c.maximumSampleGapMs=150;
    MotionDetector d(c);
    assert(!d.accept(0,0,0,30));
    assert(d.observation().phase == DetectionPhase::ImpactDetected);
    assert(!d.accept(100,0,0,9.81));
    assert(!d.accept(200,0,0,9.81));
    assert(d.accept(300,0,0,9.81));
    assert(d.observation().phase == DetectionPhase::FallConfirmed);
    d.reset(); assert(!d.accept(0,0,0,30)); assert(!d.accept(200,0,0,9.81));
    assert(d.observation().phase == DetectionPhase::Normal);
}
static void test_pressure() {
    Config c; c.pressureEvidenceEnabled=true; c.pressureMinimumSamples=3; c.pressureFilterAlpha=.5; c.pressureMinimumRisePa=10; c.pressureWindowMs=1000; c.pressureStaleAfterMs=500;
    PressureCore p(c, true);
    assert(p.status() == SensorStatus::Calibrating);
    assert(p.bootstrap({100000,100004,100002}, 0));
    assert(std::abs(*p.referencePa()-100002.0)<.001);
    p.sample(100,100000); p.sample(200,100010); p.sample(300,100020);
    auto o=p.observe(300);
    assert(o.status==SensorStatus::Ok && o.currentPa && std::abs(*o.currentPa-100012.5)<.001);
    assert(o.windowDeltaPa && std::abs(*o.windowDeltaPa-12.5)<.001);
    assert(o.altitudeDeltaM && *o.altitudeDeltaM < 0);
    assert(o.corroborated);
    o=p.observe(801); assert(o.status==SensorStatus::Stale && !o.currentPa && p.referencePa());
    p.setError(); o=p.observe(900); assert(o.status==SensorStatus::Error && !o.currentPa);
    p.sample(901,100030); o=p.observe(901); assert(o.status==SensorStatus::Ok && o.currentPa);
    const auto count=o.sampleCount; p.sample(900,100100); assert(p.observe(901).sampleCount == count); // out of order ignored
    PressureCore absent(c,false); auto a=absent.observe(0); assert(a.status==SensorStatus::Unavailable && !a.currentPa && !a.sampleCount);
    PressureCore unstable(c,true); assert(!unstable.bootstrap({100000,100020,100000},0)); assert(!unstable.referencePa());
}
static void test_pressure_never_drives_motion() {
    Config c; c.pressureEvidenceEnabled=true; c.pressureMinimumSamples=2;
    PressureCore pressure(c,true); assert(pressure.bootstrap({100000,100000},0));
    pressure.sample(10,100000); pressure.sample(20,100100);
    assert(pressure.observe(20).corroborated);
    MotionDetector motion(c);
    assert(!motion.accept(20,0,0,9.81));
    assert(motion.observation().phase == DetectionPhase::Normal);
}
static void test_apply_idempotency_busy_and_store() {
    const char* path=".build/config-store-test"; std::remove((std::string(path)+".a").c_str()); std::remove((std::string(path)+".b").c_str());
    FileDurableStore store(path); int resets=0; ApplyService svc(store,true,[&]{++resets;});
    auto r=request("b94aa842-29bb-44b2-9908-2505b10b49aa");
    auto a=svc.apply(r, AlertState::Monitoring); assert(a.ok && a.changed && a.configRevision==1 && resets==1 && store.writeCount()==1);
    auto dup=svc.apply(r, AlertState::Monitoring); assert(dup.ok && dup.configRevision==1 && store.writeCount()==1 && resets==1);
    auto collision=r; collision.config.impactAccelerationMs2=30; collision.configHash=canonicalHash(collision.profileId,collision.profileRevision,collision.config);
    assert(svc.apply(collision,AlertState::Monitoring).error==ErrorCode::RequestIdReused);
    auto same=request("92348300-7c9a-455d-8aa5-3493fe40bde9",Config{},1); assert(svc.apply(same,AlertState::Monitoring).ok); assert(store.writeCount()==1);
    auto next=request("11111111-1111-4111-8111-111111111111",collision.config,0); assert(svc.apply(next,AlertState::Monitoring).error==ErrorCode::RevisionConflict);
    next.expectedConfigRevision=1; assert(svc.apply(next,AlertState::Verifying).error==ErrorCode::BusyAlertActive); assert(store.writeCount()==1);
    auto bad=next; bad.configHash="00"; assert(svc.apply(bad,AlertState::Monitoring).error==ErrorCode::HashMismatch); assert(store.writeCount()==1);
    auto unavailable=next; unavailable.config.pressureEvidenceEnabled=true; unavailable.configHash=canonicalHash(unavailable.profileId,unavailable.profileRevision,unavailable.config);
    ApplyService noBarometer(store,false,[]{}); assert(noBarometer.apply(unavailable,AlertState::Monitoring).error==ErrorCode::SensorUnavailable);
    auto good=svc.apply(next,AlertState::Monitoring); assert(good.ok && good.configRevision==2 && resets==2);
    FileDurableStore restarted(path); ApplyService svc2(restarted,true,[]{}); auto current=svc2.current(); assert(current && current->configRevision==2 && current->configHash==next.configHash);
    assert(svc2.apply(next,AlertState::Monitoring).ok); // committing request survives reboot
    restarted.corruptNewestForTest(); FileDurableStore recovered(path); auto old=recovered.load(); assert(old && old->configRevision==1);
}
int main(){ test_hash_and_validation(); test_motion_uses_all_parameters(); test_pressure(); test_pressure_never_drives_motion(); test_apply_idempotency_busy_and_store(); std::cout<<"config core tests passed\n"; }
