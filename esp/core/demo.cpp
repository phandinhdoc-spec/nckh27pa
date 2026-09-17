#include "local_alert.h"
#include <iostream>
using namespace fallsafe;
int main() {
    LocalAlertStateMachine machine;
    // Virtual caller-owned monotonic clock: no sleeping, hardware, or transport.
    const Input script[] = {
        {Kind::Connected, 0},
        {Kind::Suspect, 0, "demo-1"},
        {Kind::Countdown, 0, "demo-1", 10000},
        {Kind::AckEvent, 1000, "demo-1"},
        {Kind::Disconnected, 2000},
        {Kind::StopBuzzer, 3000, "demo-1"},
        {Kind::Tick, 10000},
        {Kind::Safe, 11000, "demo-1"}
    };
    const char* names[] = {"MONITORING", "SUSPECTED", "VERIFYING", "LOCAL_ALERTING", "DEGRADED"};
    for (const auto& input : script) {
        const auto result = machine.handle(input);
        std::cout << "ms=" << input.now << " state=" << names[static_cast<unsigned>(machine.state())]
                  << " buzzer=" << machine.buzzer() << " actions=" << result.actions << '\n';
    }
    std::cout << "retained=" << machine.last().eventId() << " alerted=" << machine.last().alerted
              << " safe=" << machine.last().safe << '\n';
    return machine.last().alerted && machine.last().safe && !machine.active().present ? 0 : 1;
}
