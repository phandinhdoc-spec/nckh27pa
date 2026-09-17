#include "local_alert.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
using namespace fallsafe;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
static_assert(std::is_trivially_copyable<LocalAlertStateMachine>::value, "bounded value state");
static_assert(sizeof(LocalAlertStateMachine) <= 160, "bounded state size");
void suspected() {
 LocalAlertStateMachine m;
 auto r=m.handle({Kind::Suspect,0,"e"});
 CHECK(m.state()==State::SUSPECTED); CHECK(r.actions & Publish);
 CHECK(m.handle({Kind::Suspect,1,"other"}).status==Status::Busy);
 CHECK(m.active().eventId()=="e");
}
void no_response() {
 LocalAlertStateMachine m;
 CHECK(m.handle({Kind::Countdown,100,"e",10000}).status==Status::Applied);
 CHECK(m.state()==State::VERIFYING); CHECK(m.buzzer());
 m.handle({Kind::Tick,10099}); CHECK(!m.active().alerted);
 auto r=m.handle({Kind::Tick,10100}); CHECK(m.state()==State::LOCAL_ALERTING);
 CHECK(m.active().alerted); CHECK(r.actions & Publish);
 CHECK(m.handle({Kind::Tick,10101}).actions==None);
}
void sos() {
 for (int scenario=0;scenario<4;++scenario) {
 LocalAlertStateMachine m;
 if(scenario==1) m.handle({Kind::SensorError,0});
 if(scenario==2) m.handle({Kind::Suspect,0,"original"});
 if(scenario==3) m.handle({Kind::Countdown,0,"original",100});
 auto r=m.handle({Kind::Sos,1,"sos"});
 CHECK(m.state()==State::LOCAL_ALERTING); CHECK(m.buzzer()); CHECK(r.actions & Publish);
 CHECK(m.active().eventId()==(scenario>=2 ? "original" : "sos"));
 CHECK(m.handle({Kind::Sos,2,"sos"}).actions==None);
 }
}
void disconnect() {
 LocalAlertStateMachine m;
 m.handle({Kind::Connected,0}); CHECK(m.connected());
 m.handle({Kind::Countdown,1,"e",10});
 m.handle({Kind::Disconnected,5}); CHECK(!m.connected()); CHECK(m.deadline()==11);
 m.handle({Kind::Tick,11}); CHECK(m.active().alerted);
}
void duplicate() {
 LocalAlertStateMachine m;
 m.handle({Kind::Countdown,0,"e",100});
 CHECK(m.handle({Kind::Suspect,1,"e"}).status==Status::Ignored);
 m.handle({Kind::Countdown,50,"e",100}); CHECK(m.deadline()==100);
 m.handle({Kind::Countdown,60,"e",10}); CHECK(m.deadline()==70);
 CHECK(m.handle({Kind::Countdown,61,"other",1}).status==Status::Busy);
 m.handle({Kind::Tick,70}); CHECK(m.active().alerted);
 m.handle({Kind::CancelAlert,71,"e"});
 CHECK(m.handle({Kind::Countdown,72,"e",10}).status==Status::Ignored);
 CHECK(!m.active().present);
}
void cancellation() {
 LocalAlertStateMachine m; m.handle({Kind::Countdown,0,"e",10});
 for(auto k:{Kind::CancelAlert,Kind::AckEvent,Kind::StopBuzzer,Kind::Safe}) {
 CHECK(m.handle({k,1,"wrong"}).status==Status::WrongId);
 CHECK(m.active().present); CHECK(m.buzzer());
 }
 auto r=m.handle({Kind::CancelAlert,2,"e"});
 CHECK(!m.active().present); CHECK(!m.buzzer()); CHECK(r.actions & StopBuzzer);
 CHECK(m.last().cancelled); CHECK(m.last().eventId()=="e");
 CHECK(m.state()==State::MONITORING);
}
void ack_stop() {
 LocalAlertStateMachine m; m.handle({Kind::Countdown,0,"e",10});
 m.handle({Kind::AckEvent,1,"e"}); CHECK(m.active().acknowledged); CHECK(m.buzzer());
 m.handle({Kind::StopBuzzer,2,"e"}); CHECK(!m.buzzer()); CHECK(m.active().present);
 m.handle({Kind::Tick,10}); CHECK(m.buzzer()); CHECK(m.active().alerted);
 m.handle({Kind::AckEvent,11,"e"}); CHECK(m.buzzer());
 m.handle({Kind::StopBuzzer,12,"e"}); m.handle({Kind::Tick,13});
 CHECK(!m.buzzer()); CHECK(m.active().alerted);
}
void monotonic() {
 LocalAlertStateMachine m; const auto top=std::numeric_limits<std::uint64_t>::max();
 m.handle({Kind::Countdown,top-10,"e",10}); CHECK(m.deadline()==top);
 CHECK(m.handle({Kind::Tick,top-11}).status==Status::ClockReversed);
 CHECK(!m.active().alerted); m.handle({Kind::Tick,top}); CHECK(m.active().alerted);
 LocalAlertStateMachine n;
 CHECK(n.handle({Kind::Countdown,top-5,"e",10}).status==Status::Invalid);
 CHECK(!n.active().present);
}
void invalid() {
 LocalAlertStateMachine m;
 for(auto duration:{std::uint64_t{0},std::uint64_t{60001},std::numeric_limits<std::uint64_t>::max()})
 CHECK(m.handle({Kind::Countdown,0,"e",duration}).status==Status::Invalid);
 const std::string large(33,'x'); const std::string embedded("a\0b",3);
 for(auto id:{std::string{},large,embedded})
 CHECK(m.handle({Kind::Countdown,0,id,10}).status==Status::Invalid);
 CHECK(!m.active().present);
 const std::string maxId(32,'x');
 CHECK(m.handle({Kind::Countdown,0,maxId,10}).status==Status::Applied);
 CHECK(m.active().eventId()==maxId);
 CHECK(m.handle({Kind::Countdown,1,maxId,0}).status==Status::Invalid);
 CHECK(m.deadline()==10);
}
void recovery() {
 LocalAlertStateMachine m; m.handle({Kind::SensorError,0}); CHECK(m.state()==State::DEGRADED);
 m.handle({Kind::SensorRecovered,1}); CHECK(m.state()==State::MONITORING);
 m.handle({Kind::Countdown,2,"e",10}); m.handle({Kind::SensorError,3});
 CHECK(!m.sensorHealthy()); CHECK(m.state()==State::VERIFYING);
 m.handle({Kind::Tick,12}); m.handle({Kind::SensorRecovered,13});
 CHECK(m.sensorHealthy()); CHECK(m.active().alerted); CHECK(m.state()==State::LOCAL_ALERTING);
 m.handle({Kind::SensorError,14}); m.handle({Kind::CancelAlert,15,"e"}); CHECK(m.state()==State::DEGRADED);
}
void safe_record() {
 LocalAlertStateMachine m; m.handle({Kind::Countdown,0,"e",10});
 m.handle({Kind::Safe,10,"e"}); CHECK(m.last().alerted); CHECK(m.last().safe);
 CHECK(!m.active().present); CHECK(m.last().eventId()=="e");
 CHECK(m.handle({Kind::Safe,11,"e"}).status==Status::Ignored);
 CHECK(m.last().alerted);
 m.handle({Kind::Countdown,12,"next",10}); m.handle({Kind::Safe,13,"next"});
 CHECK(m.last().safe); CHECK(!m.last().alerted);
 // Post-implementation regressions requested by independent reviewer.
 for (auto kind : {Kind::Safe, Kind::CancelAlert}) {
   LocalAlertStateMachine n; n.handle({Kind::Countdown,0,"boundary",10});
   const auto result = n.handle({kind,10,"boundary"});
   CHECK(result.status == Status::Applied);
   CHECK(result.actions == (Publish | StartBuzzer | StopBuzzer | RecordUpdated));
   CHECK(!n.active().present); CHECK(!n.buzzer()); CHECK(n.last().alerted);
   CHECK(n.last().safe == (kind == Kind::Safe));
   CHECK(n.last().cancelled == (kind == Kind::CancelAlert));
 }
 LocalAlertStateMachine wrong; wrong.handle({Kind::Countdown,0,"active",10});
 const auto rejected = wrong.handle({Kind::CancelAlert,10,"other"});
 CHECK(rejected.status == Status::WrongId);
 CHECK(rejected.actions == (Publish | StartBuzzer | RecordUpdated));
 CHECK(wrong.active().eventId() == "active"); CHECK(wrong.active().alerted); CHECK(wrong.buzzer());
}
int main() {
 struct Test { const char* name; void (*run)(); };
 const Test tests[]={{"suspected",suspected},{"no-response",no_response},{"SOS",sos},{"disconnect",disconnect},{"duplicate/deadline",duplicate},{"cancel/wrong-ID",cancellation},{"ACK/STOP",ack_stop},{"monotonic/max",monotonic},{"invalid countdown/ID",invalid},{"sensor recovery",recovery},{"late SAFE record",safe_record}};
 int failures=0;
 for(const auto& t:tests) { try {t.run(); std::cout<<"PASS "<<t.name<<'\n';} catch(const std::exception& e) {++failures; std::cout<<"FAIL "<<t.name<<": "<<e.what()<<'\n';} }
 std::cout<<"tests="<<11<<" failures="<<failures<<'\n'; return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
