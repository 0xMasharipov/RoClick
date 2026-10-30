#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace roclick {
using Time = int64_t; // Monotonic nanoseconds, never wall-clock time.
constexpr Time Millisecond = 1'000'000;
enum class Button { Left, Right, Middle };
enum class TargetState { Missing, Focused, Background, Destroyed };
enum class State { Stopped, Running, Paused, Capturing, Error };
struct Config {
  int intervalMs = 10;
  int pressMs = 0;
  Button button = Button::Left;
  bool lockToTarget = true;
};
inline bool Valid(const Config &c) {
  return c.intervalMs >= 1 && c.intervalMs <= 1000 && c.pressMs >= 0 &&
    c.pressMs < c.intervalMs && c.button >= Button::Left && c.button <= Button::Middle;
}
struct Submission { unsigned count; uint32_t error; bool cancelled = false; };
struct Platform {
  virtual ~Platform() = default;
  virtual Time Now() = 0;
  virtual TargetState Target() = 0;
  virtual bool Capture() = 0;
  virtual void ClearTarget() = 0;
  // pair=true means one ordered SendInput([down, up]), never multiple clicks.
  virtual Submission Input(Button button, bool down, bool pair) = 0;
};
struct Snapshot {
  Config config;
  State state = State::Stopped;
  uint64_t clicks = 0, inputEvents = 0, skipped = 0, failures = 0;
  uint64_t gapCount = 0;
  double gapMeanMs = 0, gapM2 = 0, gapMinMs = 0, gapMaxMs = 0;
  Time started = 0, ended = 0;
  uint32_t errorCode = 0;
  std::string error;
  bool held = false;
};

// Single-owner state machine. Windows runtime owns it exclusively on its worker.
// Tests supply virtual time, focus and partial SendInput results.
class Engine {
 public:
  explicit Engine(Platform &platform) : p_(platform) {}
  const Snapshot &Get() const { return s_; }
  bool Active() const { return s_.state == State::Running || s_.state == State::Paused; }
  void Configure(Config c) {
    if (!Valid(c)) { Fail("Use an integer interval of 1–1000 ms and a press duration below the interval."); return; }
    const bool resume = Active();
    if (!Release()) return;
    s_.config = c;
    next_ = p_.Now() + Interval(); // Reconfiguration cannot create a catch-up click.
    lastClick_ = -1;
    if (resume) CheckTarget();
  }
  void Start() {
    if (Active() || s_.state == State::Capturing) return;
    if (!Release()) return;
    if (recaptureRequired_) { Fail("The captured window closed. Capture a target again before starting."); return; }
    const auto config = s_.config;
    s_ = Snapshot{};
    s_.config = config;
    s_.started = p_.Now();
    s_.state = State::Running;
    lastClick_ = -1;
    next_ = p_.Now();
    CheckTarget();
  }
  void Stop() {
    const bool released = Release();
    captureAt_ = 0;
    s_.ended = p_.Now();
    if (released && s_.state != State::Error) s_.state = State::Stopped;
  }
  void CaptureAfter(int delayMs) {
    Stop();
    if (s_.held) return;
    s_.error.clear(); s_.errorCode = 0;
    p_.ClearTarget();
    captureAt_ = p_.Now() + std::clamp(delayMs, 500, 10000) * Millisecond;
    s_.state = State::Capturing;
  }
  void ClearTarget() { Stop(); p_.ClearTarget(); }
  void ExternalFailure(std::string message, uint32_t code = 0) { Fail(std::move(message), code); }
  void Tick() {
    auto now = p_.Now();
    if (s_.state == State::Capturing) {
      if (now >= captureAt_) {
        if (p_.Capture()) { s_.state = State::Stopped; recaptureRequired_ = false; }
        else Fail("Could not capture this window. Focus another desktop application and capture again.");
      }
      return;
    }
    // Detect target destruction even while stopped or unlocked.
    if (p_.Target() == TargetState::Destroyed) {
      recaptureRequired_ = true;
      p_.ClearTarget(); Fail("The captured window closed. Capture a target again before starting."); return;
    }
    if (!Active()) return;
    if (!CheckTarget()) return;
    now = p_.Now();
    if (s_.held) {
      if (now < releaseAt_) return;
      if (!Release()) return;
      Complete(p_.Now());
      Advance(p_.Now());
      return; // Never release one press and begin another in the same turn.
    }
    if (now < next_) return;
    // A late wake produces at most ONE click, with all elapsed slots discarded.
    if (now - next_ >= Interval()) {
      const auto missed = (now - next_) / Interval();
      s_.skipped += static_cast<uint64_t>(missed);
      next_ += missed * Interval();
    }
    const bool pair = s_.config.pressMs == 0;
    const auto sent = p_.Input(s_.config.button, true, pair);
    if (sent.cancelled) { CheckTarget(); return; }
    s_.inputEvents += sent.count;
    // SendInput inserts events in order; a partial pair leaves the down held.
    s_.held = sent.count == 1;
    heldButton_ = s_.config.button;
    if (sent.count != (pair ? 2u : 1u)) {
      Fail("Windows rejected mouse input. Stop, check the target's permissions or input restrictions, and retry. RoClick does not elevate automatically.", sent.error);
      return;
    }
    if (pair) { Complete(p_.Now()); Advance(p_.Now()); }
    else releaseAt_ = p_.Now() + s_.config.pressMs * Millisecond;
  }
  Time NextWake() {
    const auto poll = p_.Now() + 10 * Millisecond;
    if (s_.state == State::Capturing) return std::min(poll, captureAt_);
    if (s_.state == State::Running) return std::min(poll, s_.held ? releaseAt_ : next_);
    return poll;
  }
 private:
  Time Interval() const { return s_.config.intervalMs * Millisecond; }
  bool Release() {
    if (!s_.held) return true;
    const auto result = p_.Input(heldButton_, false, false);
    s_.inputEvents += result.count;
    if (result.count == 1) { s_.held = false; return true; }
    ++s_.failures; s_.errorCode = result.error;
    s_.error = "Windows rejected a button release. Release the physical mouse button, check desktop/target permissions, then Stop to retry the release.";
    s_.state = State::Error; s_.ended = p_.Now();
    return false; // Keep held=true so Stop/shutdown retry; never start another down.
  }
  void Fail(std::string message, uint32_t code = 0) {
    ++s_.failures;
    if (!Release()) return;
    s_.error = std::move(message); s_.errorCode = code;
    s_.state = State::Error; s_.ended = p_.Now(); captureAt_ = 0;
  }
  bool CheckTarget() {
    const auto target = p_.Target();
    if (target == TargetState::Destroyed) {
      recaptureRequired_ = true;
      p_.ClearTarget(); Fail("The captured window closed. Capture a target again before starting."); return false;
    }
    if (!s_.config.lockToTarget) return true;
    if (target == TargetState::Missing) { Fail("Capture a target first, or turn off target focus lock in Advanced."); return false; }
    if (target == TargetState::Background) {
      if (!Release()) return false;
      s_.state = State::Paused;
      lastClick_ = -1;
      next_ = p_.Now() + Interval();
      return false;
    }
    if (s_.state == State::Paused) { s_.state = State::Running; next_ = p_.Now() + Interval(); }
    return true;
  }
  void Complete(Time now) {
    ++s_.clicks;
    if (lastClick_ >= 0) {
      const double gap = double(now - lastClick_) / Millisecond;
      ++s_.gapCount;
      const double delta = gap - s_.gapMeanMs;
      s_.gapMeanMs += delta / double(s_.gapCount);
      s_.gapM2 += delta * (gap - s_.gapMeanMs);
      s_.gapMinMs = s_.gapCount == 1 ? gap : std::min(s_.gapMinMs, gap);
      s_.gapMaxMs = std::max(s_.gapMaxMs, gap);
    }
    lastClick_ = now;
  }
  void Advance(Time now) {
    next_ += Interval();
    if (next_ <= now) {
      const auto missed = (now - next_) / Interval() + 1;
      s_.skipped += static_cast<uint64_t>(missed);
      next_ += missed * Interval();
    }
    // Keep phase for small wake jitter, but never squeeze a catch-up interval
    // below 90% of the configured period after a delayed submission.
    if (next_ - now < Interval() * 9 / 10) next_ = now + Interval();
  }
  Platform &p_;
  Snapshot s_;
  Button heldButton_ = Button::Left;
  Time next_ = 0, releaseAt_ = 0, captureAt_ = 0, lastClick_ = -1;
  bool recaptureRequired_ = false;
};
} // namespace roclick
