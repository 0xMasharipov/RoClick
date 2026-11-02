#include "Engine.h"
#include "RobloxTarget.h"
#include <cstdlib>
#include <deque>
#include <iostream>
#include <vector>
using namespace roclick;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << "\n"; std::exit(1); } } while(false)
struct Fake : Platform {
  Time time = 0;
  TargetState target = TargetState::Focused;
  bool captureOK = true;
  TargetState captureState = TargetState::Focused;
  int captures = 0;
  int targetChecks = 0;
  std::deque<Submission> results;
  struct Call { Button button; bool down, pair; Time time; };
  std::vector<Call> calls;
  Time Now() override { return time; }
  TargetState Target() override { ++targetChecks; return target; }
  bool Capture() override { ++captures; target = captureOK ? captureState : TargetState::Missing; return captureOK; }
  void ClearTarget() override { target = TargetState::Missing; }
  Submission Input(Button b, bool d, bool p) override {
    calls.push_back({b,d,p,time});
    if (results.empty()) return {p ? 2u : 1u, 0};
    auto result = results.front(); results.pop_front(); return result;
  }
  void At(int ms, Engine &e) { time = ms * Millisecond; e.Tick(); }
};
// Existing throughput/failure scenarios use explicit unrestricted timing.
struct TestEngine : Engine {
  explicit TestEngine(Platform &p) : Engine(p) { Configure({10,0,Button::Left,true,false}); }
};
int main() {
  { Fake p; Engine e(p);
    CHECK(e.Get().config.gameMode); CHECK(e.Get().config.intervalMs==50);
    e.Start();
    for (int ms=0; ms<60000; ++ms) p.At(ms,e);
    CHECK(e.Get().clicks==1200); CHECK(e.Get().failures==0); }
  // The native limit applies even if a caller requests 1 ms directly. Slower
  // user choices remain slower; changing buttons does not bypass the limit.
  for (int interval : {1,2,10,50,100,1000}) {
    Fake p; Engine e(p); e.Configure({interval,0,Button::Right,true,true}); e.Start();
    for (int ms=0; ms<60000; ++ms) p.At(ms,e);
    CHECK(e.Get().clicks==uint64_t(60000/std::max(50,interval)));
    for (size_t i=1; i<p.calls.size(); ++i)
      CHECK(p.calls[i].time-p.calls[i-1].time>=std::max(50,interval)*Millisecond);
  }
  { Fake p; Engine e(p); e.Configure({1,0,Button::Left,true,true}); e.Start();
    for (int ms : {0,55,100,105,106,107,109,110,111,200,201,202,250,251,5000,5001,5050}) p.At(ms,e);
    for (size_t i=1; i<p.calls.size(); ++i) CHECK(p.calls[i].time-p.calls[i-1].time>=50*Millisecond);
    CHECK(e.Get().skipped>0);
    p.target=TargetState::Background; p.At(5060,e); CHECK(e.Get().state==State::Paused);
    p.target=TargetState::Focused; p.At(6000,e);
    const auto clicks=e.Get().clicks; p.At(6049,e); CHECK(e.Get().clicks==clicks);
    p.At(6050,e); CHECK(e.Get().clicks==clicks+1); }
  { Fake p; Engine e(p); e.Configure({10,5,Button::Right,true,false}); e.Start(); e.Tick();
    CHECK(e.Get().held); p.time=2*Millisecond;
    e.Configure({1,0,Button::Middle,true,true});
    CHECK(!e.Get().held); CHECK(p.calls.back().button==Button::Right); CHECK(!p.calls.back().down);
    p.At(51,e); CHECK(e.Get().clicks==0); p.At(52,e); CHECK(e.Get().clicks==1);
    CHECK(p.calls.back().button==Button::Middle);
    e.Configure({1,0,Button::Left,true,false}); p.At(53,e); CHECK(e.Get().clicks==2);
    e.Stop(); p.At(100,e); CHECK(e.Get().clicks==2); }
  CHECK(IsRobloxPlayer(L"C:\\Users\\Player\\RobloxPlayerBeta.exe"));
  CHECK(IsRobloxPlayer(L"C:/Roblox/ROBLOXPLAYER.EXE"));
  CHECK(IsRobloxPlayer(L"Windows10Universal.exe"));
  for (const auto path : {L"", L"chrome.exe", L"RobloxPlayerLauncher.exe", L"RobloxStudioBeta.exe",
      L"FakeRobloxPlayerBeta.exe", L"RobloxPlayerBeta.exe.bak", L"C:\\RobloxPlayerBeta.exe\\other.exe"}) CHECK(!IsRobloxPlayer(path));
  { Fake p; TestEngine e(p); p.target=TargetState::Missing; p.captureOK=false;
    CHECK(!e.TryCaptureTarget()); CHECK(e.Get().state==State::Stopped); CHECK(e.Get().failures==0);
    p.captureOK=true; p.captureState=TargetState::Background;
    CHECK(e.TryCaptureTarget()); CHECK(e.Get().state==State::Stopped); CHECK(p.calls.empty());
    e.Start(); CHECK(e.Get().state==State::Paused); CHECK(p.calls.empty());
    CHECK(!e.TryCaptureTarget()); CHECK(p.captures==2); }
  { Fake p; TestEngine e(p); e.Start(); p.target=TargetState::Destroyed; e.Tick();
    CHECK(e.Get().errorKind==ErrorKind::TargetClosed);
    CHECK(e.TryCaptureTarget()); CHECK(e.Get().errorKind==ErrorKind::None); CHECK(e.Get().state==State::Stopped);
    CHECK(p.calls.empty()); e.Start(); CHECK(e.Get().state==State::Running); }
  { Fake p; TestEngine e(p); p.target=TargetState::Missing; e.CaptureAfter(0); e.Tick();
    CHECK(e.Get().state==State::Stopped); CHECK(p.target==TargetState::Focused); CHECK(p.calls.empty()); }
  { Fake p; TestEngine e(p); e.CaptureAfter(500);
    CHECK(!e.TryCaptureTarget()); CHECK(p.captures==0); }
  for (auto kind : {ErrorKind::InputRejected, ErrorKind::ReleaseRejected, ErrorKind::InvalidConfig,
      ErrorKind::MonitoringUnavailable, ErrorKind::TimerUnavailable, ErrorKind::SchedulerWaitFailed}) {
    Fake p; TestEngine e(p); p.target=TargetState::Missing; e.ExternalFailure(kind,"preserve",5);
    CHECK(!e.TryCaptureTarget()); CHECK(p.captures==0); CHECK(e.Get().errorKind==kind); CHECK(e.Get().errorCode==5);
  }
  CHECK(Valid({1,0,Button::Left,true})); CHECK(Valid({1000,999,Button::Middle,false}));
  CHECK(!Valid({0,0,Button::Left,true})); CHECK(!Valid({1001,0,Button::Left,true}));
  CHECK(!Valid({10,10,Button::Left,true})); CHECK(!Valid({10,-1,Button::Left,true}));
  { Fake p; TestEngine e(p);
    CHECK(e.NextWake() == 1000*Millisecond);
    e.Start(); p.targetChecks=0; e.Tick();
    CHECK(p.targetChecks==1); CHECK(e.Get().clicks==1);
    CHECK(e.NextWake() <= 10*Millisecond);
    e.Stop(); CHECK(e.NextWake() == 1000*Millisecond);
    e.CaptureAfter(500); CHECK(e.NextWake() == 500*Millisecond);
    e.Stop(); p.At(500,e); CHECK(p.captures==0); }
  for (int interval : {1,2,10,100,1000}) {
    Fake p; TestEngine e(p); e.Configure({interval,0,Button::Left,true,false}); e.Start();
    for (int ms = 0; ms < 60000; ++ms) p.At(ms,e);
    CHECK(e.Get().clicks == uint64_t(60000/interval)); CHECK(e.Get().skipped == 0);
  }
  { Fake p; TestEngine e(p); e.Start(); e.Tick(); p.At(105,e);
    CHECK(e.Get().clicks == 2); CHECK(e.Get().skipped == 9);
    for (int i=0;i<10;++i) { e.Tick(); }
    CHECK(e.Get().clicks == 2);
    CHECK(e.NextWake() >= 114*Millisecond); }
  { Fake p; TestEngine e(p); e.Configure({10,5,Button::Right,true,false}); e.Start(); e.Tick();
    CHECK(e.Get().held); p.time=2*Millisecond; e.Stop();
    CHECK(!e.Get().held); CHECK(!p.calls.back().down); CHECK(p.calls.back().button==Button::Right);
    CHECK(e.Get().clicks==0); p.At(100,e); CHECK(p.calls.size()==2); }
  { Fake p; TestEngine e(p); e.Configure({10,5,Button::Right,true,false}); e.Start(); e.Tick();
    e.Configure({2,0,Button::Middle,true,false}); CHECK(!e.Get().held);
    CHECK(p.calls.back().button==Button::Right); p.At(2,e);
    CHECK(p.calls.back().button==Button::Middle); CHECK(e.Get().clicks==1); }
  { Fake p; TestEngine e(p); p.results={{1,5},{1,0}}; e.Start(); e.Tick();
    CHECK(e.Get().state==State::Error); CHECK(!e.Get().held); CHECK(e.Get().clicks==0);
    CHECK(e.Get().inputEvents==2); CHECK(e.Get().errorCode==5); CHECK(e.Get().errorKind==ErrorKind::InputRejected); CHECK(p.calls.size()==2); }
  { Fake p; TestEngine e(p); p.results={{0,0}}; e.Start(); e.Tick();
    CHECK(e.Get().state==State::Error); CHECK(p.calls.size()==1); CHECK(e.Get().clicks==0); }
  { Fake p; TestEngine e(p); p.results={{1,0},{0,5},{0,5},{1,0}};
    e.Start(); e.Tick(); CHECK(e.Get().held); e.Stop(); CHECK(e.Get().held);
    e.Stop(); CHECK(!e.Get().held); CHECK(e.Get().state==State::Error); }
  { Fake p; TestEngine e(p); e.Start(); e.Tick(); p.target=TargetState::Background; p.At(10,e);
    CHECK(e.Get().state==State::Paused); p.At(1000,e); CHECK(e.Get().clicks==1);
    p.target=TargetState::Focused; e.Tick(); CHECK(e.Get().clicks==1); p.At(1010,e); CHECK(e.Get().clicks==2);
    p.target=TargetState::Destroyed; p.At(1011,e); CHECK(e.Get().state==State::Error);
    CHECK(p.target==TargetState::Missing); e.Start(); CHECK(e.Get().state==State::Error); }
  { Fake p; TestEngine e(p); e.Configure({100,90,Button::Left,true,false}); e.Start(); e.Tick();
    p.target=TargetState::Background; p.At(1,e); CHECK(!e.Get().held); CHECK(e.Get().state==State::Paused); }
  { Fake p; TestEngine e(p); e.CaptureAfter(1000); p.At(999,e); CHECK(e.Get().state==State::Capturing);
    e.Stop(); p.At(1000,e); CHECK(p.target==TargetState::Missing);
    e.CaptureAfter(500); p.At(1500,e); CHECK(e.Get().state==State::Stopped); CHECK(p.target==TargetState::Focused); }
  { Fake p; TestEngine e(p); e.Start(); e.Configure({1,1,Button::Left,true,false}); CHECK(!e.Active()); }
  { Fake p; TestEngine e(p); e.Configure({10,5,Button::Middle,true,false}); e.Start(); e.Tick();
    p.At(4,e); CHECK(e.Get().held); CHECK(e.Get().clicks==0);
    p.At(5,e); CHECK(!e.Get().held); CHECK(e.Get().clicks==1); CHECK(e.Get().inputEvents==2);
    p.At(9,e); CHECK(p.calls.size()==2); p.At(10,e); CHECK(e.Get().held); }
  { Fake p; TestEngine e(p); e.Configure({10,5,Button::Left,true,false}); p.results={{1,0},{0,5},{1,0}};
    e.Start(); e.Tick(); p.At(5,e); CHECK(e.Get().held); CHECK(e.Get().state==State::Error);
    e.Stop(); CHECK(!e.Get().held); CHECK(e.Get().errorKind==ErrorKind::ReleaseRejected); CHECK(e.Get().clicks==0); }
  { Fake p; TestEngine e(p); e.Configure({10,0,Button::Left,false,false}); e.Start(); e.Tick();
    p.target=TargetState::Destroyed; e.Tick(); CHECK(e.Get().state==State::Error);
    e.Start(); CHECK(e.Get().state==State::Error); // Unlocked still requires recapture.
    e.CaptureAfter(500); p.At(500,e); e.Start(); e.Tick(); CHECK(e.Get().clicks==1); }
  { Fake p; TestEngine e(p); p.captureOK=false; e.CaptureAfter(500); p.At(500,e);
    CHECK(e.Get().state==State::Error); CHECK(e.Get().errorKind==ErrorKind::CaptureFailed); CHECK(p.calls.empty()); }
  { Fake p; TestEngine e(p); e.Start();
    // Irregular wakes under simulated load: bounded gaps, no replay at the same time.
    for(int ms : {0,10,35,36,37,45,200,201,202,210,220}) p.At(ms,e);
    for(size_t i=1;i<p.calls.size();++i) CHECK(p.calls[i].time-p.calls[i-1].time >= 9*Millisecond);
    CHECK(e.Get().skipped>0); }
  { Fake p; TestEngine e(p); p.results={{0,0,true}}; e.Start(); e.Tick();
    CHECK(e.Get().failures==0); CHECK(e.Get().clicks==0); e.Stop(); CHECK(!e.Active()); }
  { Fake p; TestEngine e(p); e.Configure({0,0,Button::Left,true,false});
    CHECK(e.Get().errorKind==ErrorKind::InvalidConfig);
    e.CaptureAfter(500); CHECK(e.Get().errorKind==ErrorKind::None); }
  { Fake p; TestEngine e(p); p.target=TargetState::Missing; e.Start();
    CHECK(e.Get().errorKind==ErrorKind::TargetMissing); }
  { Fake p; TestEngine e(p); e.Start(); p.target=TargetState::Destroyed; e.Tick();
    CHECK(e.Get().errorKind==ErrorKind::TargetClosed); }
  for (auto kind : {ErrorKind::MonitoringUnavailable, ErrorKind::TimerUnavailable, ErrorKind::SchedulerWaitFailed}) {
    Fake p; TestEngine e(p); e.ExternalFailure(kind, "diagnostic", 5);
    CHECK(e.Get().errorKind==kind); CHECK(e.Get().errorCode==5); CHECK(e.Get().error=="diagnostic");
  }
  CHECK(std::string(ErrorKey(ErrorKind::None)).empty());
  std::cout << "Engine tests passed (virtual-time 60 s schedules, cancellation, reconfigure, partial failures, focus, destruction, capture).\n";
}
