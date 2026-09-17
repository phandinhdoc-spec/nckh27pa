#ifndef FALLSAFE_LOCAL_ALERT_H
#define FALLSAFE_LOCAL_ALERT_H
#include <array>
#include <cstdint>
#include <string_view>
namespace fallsafe {
enum class State { MONITORING, SUSPECTED, VERIFYING, LOCAL_ALERTING, DEGRADED };
enum class Kind { Suspect, Countdown, Sos, Tick, AckEvent, StopBuzzer, CancelAlert, Safe, SensorError, SensorRecovered, Connected, Disconnected };
enum class Status { Applied, Ignored, Invalid, Busy, WrongId, ClockReversed };
enum Action : unsigned { None = 0, Publish = 1, StartBuzzer = 2, StopBuzzer = 4, RecordUpdated = 8, SensorChanged = 16 };
struct Input { Kind kind; std::uint64_t now; std::string_view eventId{}; std::uint64_t remainingMs = 0; };
struct Result { Status status; unsigned actions; };
struct Record {
    std::array<char, 33> id{};
    bool present = false;
    bool acknowledged = false;
    bool alerted = false;
    bool safe = false;
    bool cancelled = false;
    std::string_view eventId() const noexcept { return {id.data()}; }
};
class LocalAlertStateMachine {
public:
    static constexpr std::uint64_t maxCountdownMs = 60000;
    Result handle(const Input& input) noexcept;
    State state() const noexcept { return state_; }
    const Record& active() const noexcept { return active_; }
    const Record& last() const noexcept { return last_; }
    bool buzzer() const noexcept { return buzzer_; }
    bool sensorHealthy() const noexcept { return healthy_; }
    bool connected() const noexcept { return connected_; }
    std::uint64_t deadline() const noexcept { return deadline_; }
private:
    State state_ = State::MONITORING;
    Record active_{}, last_{};
    bool buzzer_ = false, healthy_ = true, connected_ = false;
    bool clockSet_ = false, timed_ = false;
    std::uint64_t now_ = 0, deadline_ = 0;
    void alert(unsigned& actions) noexcept;
    void finish(bool safe, unsigned& actions) noexcept;
};
}
#endif
