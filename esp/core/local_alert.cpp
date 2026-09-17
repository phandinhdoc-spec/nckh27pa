#include "local_alert.h"
#include <algorithm>
#include <limits>
namespace fallsafe {
namespace {
bool validId(std::string_view id) noexcept {
    return !id.empty() && id.size() <= 32 && id.find('\0') == std::string_view::npos;
}
}
void LocalAlertStateMachine::alert(unsigned& actions) noexcept {
    if (active_.alerted) return;
    active_.alerted = true;
    timed_ = false;
    state_ = State::LOCAL_ALERTING;
    buzzer_ = true;
    actions |= Publish | StartBuzzer | RecordUpdated;
}
void LocalAlertStateMachine::finish(bool safe, unsigned& actions) noexcept {
    active_.safe = safe;
    active_.cancelled = !safe;
    last_ = active_;
    active_ = {};
    timed_ = false;
    deadline_ = 0;
    if (buzzer_) actions |= StopBuzzer;
    buzzer_ = false;
    state_ = healthy_ ? State::MONITORING : State::DEGRADED;
    actions |= RecordUpdated | Publish;
}
Result LocalAlertStateMachine::handle(const Input& input) noexcept {
    if (clockSet_ && input.now < now_) return {Status::ClockReversed, None};
    clockSet_ = true;
    now_ = input.now;
    unsigned actions = None;
    // Process elapsed time first, including when a SAFE arrives at the deadline.
    if (timed_ && now_ >= deadline_) alert(actions);
    const auto result = [&actions](Status status) { return Result{status, actions}; };
    switch (input.kind) {
    case Kind::Tick: return result(Status::Applied);
    case Kind::Connected:
    case Kind::Disconnected:
        connected_ = input.kind == Kind::Connected;
        return result(Status::Applied);
    case Kind::SensorError:
    case Kind::SensorRecovered: {
        const bool healthy = input.kind == Kind::SensorRecovered;
        if (healthy == healthy_) return result(Status::Ignored);
        healthy_ = healthy;
        actions |= SensorChanged;
        if (!active_.present) state_ = healthy_ ? State::MONITORING : State::DEGRADED;
        return result(Status::Applied);
    }
    case Kind::Suspect:
    case Kind::Countdown:
    case Kind::Sos: {
        if (!validId(input.eventId)) return result(Status::Invalid);
        if (input.kind == Kind::Countdown &&
            (input.remainingMs == 0 || input.remainingMs > maxCountdownMs ||
             input.remainingMs > std::numeric_limits<std::uint64_t>::max() - now_))
            return result(Status::Invalid);
        if (active_.present) {
            // A physical SOS escalates the existing event, preserving its identity.
            if (input.kind == Kind::Sos) {
                const bool already = active_.alerted;
                alert(actions);
                return result(already ? Status::Ignored : Status::Applied);
            }
            if (active_.eventId() != input.eventId) return result(Status::Busy);
            if (input.kind == Kind::Suspect || active_.alerted) return result(Status::Ignored);
        } else {
            if (last_.present && last_.eventId() == input.eventId) return result(Status::Ignored);
            active_ = {};
            std::copy(input.eventId.begin(), input.eventId.end(), active_.id.begin());
            active_.present = true;
            actions |= Publish | RecordUpdated;
        }
        if (input.kind == Kind::Sos) {
            alert(actions);
        } else if (input.kind == Kind::Suspect) {
            state_ = State::SUSPECTED;
        } else {
            const auto candidate = now_ + input.remainingMs;
            if (timed_) {
                if (candidate >= deadline_) return result(Status::Ignored);
                deadline_ = candidate;
            } else {
                deadline_ = candidate;
                timed_ = true;
                state_ = State::VERIFYING;
                buzzer_ = true;
                actions |= StartBuzzer;
            }
        }
        return result(Status::Applied);
    }
    case Kind::AckEvent:
    case Kind::StopBuzzer:
    case Kind::CancelAlert:
    case Kind::Safe:
        if (!validId(input.eventId)) return result(Status::Invalid);
        if (!active_.present) {
            if (last_.present && last_.eventId() == input.eventId) return result(Status::Ignored);
            return result(Status::WrongId);
        }
        if (active_.eventId() != input.eventId) return result(Status::WrongId);
        if (input.kind == Kind::AckEvent) {
            if (active_.acknowledged) return result(Status::Ignored);
            active_.acknowledged = true;
            actions |= RecordUpdated;
        } else if (input.kind == Kind::StopBuzzer) {
            if (!buzzer_) return result(Status::Ignored);
            buzzer_ = false;
            actions |= StopBuzzer;
        } else {
            finish(input.kind == Kind::Safe, actions);
        }
        return result(Status::Applied);
    }
    return result(Status::Invalid);
}
}
