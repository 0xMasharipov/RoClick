#include "Engine.h"
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
  std::deque<Submission> results;
  struct Call { Button button; bool down, pair; Time time; };
  std::vector<Call> calls;
  Time Now() override { return time; }
  TargetState Target() override { return target; }
  bool Capture() override { target = captureOK ? TargetState::Focused : TargetState::Missing; return captureOK; }
  void ClearTarget() override { target = TargetState::Missing; }
  Submission Input(Button b, bool d, bool p) override {
    calls.push_back({b,d,p,time});
    if (results.empty()) return {p ? 2u : 1u, 0};
    auto result = results.front(); results.pop_front(); return result;
  }
  void At(int ms, Engine &e) { time = ms * Millisecond; e.Tick(); }
};
int main() {
  CHECK(Valid({1,0,Button::Left,true})); CHECK(Valid({1000,999,Button::Middle,false}));
  CHECK(!Valid({0,0,Button::Left,true})); CHECK(!Valid({1001,0,Button::Left,true}));
  CHECK(!Valid({10,10,Button::Left,true})); CHECK(!Valid({10,-1,Button::Left,true}));
  for (int interval : {1,2,10,100,1000}) {
    Fake p; Engine e(p); e.Configure({interval,0,Button::Left,true}); e.Start();
    for (int ms = 0; ms < 60000; ++ms) p.At(ms,e);
    CHECK(e.Get().clicks == uint64_t(60000/interval)); CHECK(e.Get().skipped == 0);
  }
  { Fake p; Engine e(p); e.Start(); e.Tick(); p.At(105,e);
    CHECK(e.Get().clicks == 2); CHECK(e.Get().skipped == 9);
    for (int i=0;i<10;++i) { e.Tick(); }
    CHECK(e.Get().clicks == 2);
    CHECK(e.NextWake() >= 114*Millisecond); }
  { Fake p; Engine e(p); e.Configure({10,5,Button::Right,true}); e.Start(); e.Tick();
    CHECK(e.Get().held); p.time=2*Millisecond; e.Stop();
    CHECK(!e.Get().held); CHECK(!p.calls.back().down); CHECK(p.calls.back().button==Button::Right);
    CHECK(e.Get().clicks==0); p.At(100,e); CHECK(p.calls.size()==2); }
  { Fake p; Engine e(p); e.Configure({10,5,Button::Right,true}); e.Start(); e.Tick();
    e.Configure({2,0,Button::Middle,true}); CHECK(!e.Get().held);
    CHECK(p.calls.back().button==Button::Right); p.At(2,e);
    CHECK(p.calls.back().button==Button::Middle); CHECK(e.Get().clicks==1); }
  { Fake p; Engine e(p); p.results={{1,5},{1,0}}; e.Start(); e.Tick();
    CHECK(e.Get().state==State::Error); CHECK(!e.Get().held); CHECK(e.Get().clicks==0);
    CHECK(e.Get().inputEvents==2); CHECK(e.Get().errorCode==5); CHECK(p.calls.size()==2); }
  { Fake p; Engine e(p); p.results={{0,0}}; e.Start(); e.Tick();
    CHECK(e.Get().state==State::Error); CHECK(p.calls.size()==1); CHECK(e.Get().clicks==0); }
  { Fake p; Engine e(p); p.results={{1,0},{0,5},{0,5},{1,0}};
    e.Start(); e.Tick(); CHECK(e.Get().held); e.Stop(); CHECK(e.Get().held);
    e.Stop(); CHECK(!e.Get().held); CHECK(e.Get().state==State::Error); }
  { Fake p; Engine e(p); e.Start(); e.Tick(); p.target=TargetState::Background; p.At(10,e);
    CHECK(e.Get().state==State::Paused); p.At(1000,e); CHECK(e.Get().clicks==1);
    p.target=TargetState::Focused; e.Tick(); CHECK(e.Get().clicks==1); p.At(1010,e); CHECK(e.Get().clicks==2);
    p.target=TargetState::Destroyed; p.At(1011,e); CHECK(e.Get().state==State::Error);
    CHECK(p.target==TargetState::Missing); e.Start(); CHECK(e.Get().state==State::Error); }
  { Fake p; Engine e(p); e.Configure({100,90,Button::Left,true}); e.Start(); e.Tick();
    p.target=TargetState::Background; p.At(1,e); CHECK(!e.Get().held); CHECK(e.Get().state==State::Paused); }
  { Fake p; Engine e(p); e.CaptureAfter(1000); p.At(999,e); CHECK(e.Get().state==State::Capturing);
    e.Stop(); p.At(1000,e); CHECK(p.target==TargetState::Missing);
    e.CaptureAfter(500); p.At(1500,e); CHECK(e.Get().state==State::Stopped); CHECK(p.target==TargetState::Focused); }
  { Fake p; Engine e(p); e.Start(); e.Configure({1,1,Button::Left,true}); CHECK(!e.Active()); }
  std::cout << "Engine tests passed (virtual-time 60 s schedules, cancellation, reconfigure, partial failures, focus, destruction, capture).\n";
}
