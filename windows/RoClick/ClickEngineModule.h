#pragma once
#include "NativeModules.h"
#include "../../native/windows/Runtime.h"
#include "WindowAppearance.h"
#include <condition_variable>

namespace RoClick {
REACT_STRUCT(ClickConfig)
struct ClickConfig {
  REACT_FIELD(intervalMs) double intervalMs = 10;
  REACT_FIELD(pressMs) double pressMs = 0;
  REACT_FIELD(button) std::string button = "left";
  REACT_FIELD(lockToTarget) bool lockToTarget = true;
};
REACT_STRUCT(EngineEvent)
struct EngineEvent {
  REACT_FIELD(state) std::string state;
  REACT_FIELD(config) ClickConfig config;
  REACT_FIELD(targetTitle) std::string targetTitle;
  REACT_FIELD(targetPid) double targetPid = 0;
  REACT_FIELD(submittedClicks) double submittedClicks = 0;
  REACT_FIELD(submittedEvents) double submittedEvents = 0;
  REACT_FIELD(submittedRate) double submittedRate = 0;
  REACT_FIELD(skipped) double skipped = 0;
  REACT_FIELD(failures) double failures = 0;
  REACT_FIELD(error) std::string error;
  REACT_FIELD(errorCode) double errorCode = 0;
  REACT_FIELD(held) bool held = false;
  REACT_FIELD(highResolution) bool highResolution = false;
  REACT_FIELD(acrylic) bool acrylic = false;
  REACT_FIELD(dark) bool dark = false;
  REACT_FIELD(highContrast) bool highContrast = false;
  REACT_FIELD(windowColor) std::string windowColor;
  REACT_FIELD(textColor) std::string textColor;
  REACT_FIELD(buttonColor) std::string buttonColor;
  REACT_FIELD(buttonTextColor) std::string buttonTextColor;
  REACT_FIELD(highlightColor) std::string highlightColor;
  REACT_FIELD(highlightTextColor) std::string highlightTextColor;
};
REACT_TURBO_MODULE(ClickEngineModule, L"ClickEngine")
struct ClickEngineModule {
  REACT_EVENT(onState) std::function<void(EngineEvent)> onState;
  REACT_INIT(Initialize)
  void Initialize(winrt::Microsoft::ReactNative::ReactContext const &) noexcept {
    reporter_ = std::thread([this] {
      std::unique_lock lock(mutex_);
      while (!cv_.wait_for(lock, std::chrono::milliseconds(250), [this] { return quit_; })) {
        if (!observing_ || pending_.exchange(true)) continue;
        const auto r = roclick::Runtime::Shared().Read();
        const auto &s = r.engine;
        EngineEvent e;
        const char *states[] = {"stopped", "running", "paused", "capturing", "error"};
        const char *buttons[] = {"left", "right", "middle"};
        e.state = states[static_cast<int>(s.state)];
        e.config = {double(s.config.intervalMs), double(s.config.pressMs), buttons[static_cast<int>(s.config.button)], s.config.lockToTarget};
        e.targetTitle = winrt::to_string(r.targetTitle); e.targetPid = r.targetPid;
        e.submittedClicks = double(s.clicks); e.submittedEvents = double(s.inputEvents); e.submittedRate = r.rate;
        e.skipped = double(s.skipped); e.failures = double(s.failures); e.error = s.error; e.errorCode = s.errorCode;
        e.held = s.held; e.highResolution = r.highResolution;
        e.acrylic = roclick::appearance::acrylic; e.dark = roclick::appearance::dark; e.highContrast = roclick::appearance::highContrast;
        e.windowColor = roclick::appearance::Color(COLOR_WINDOW); e.textColor = roclick::appearance::Color(COLOR_WINDOWTEXT);
        e.buttonColor = roclick::appearance::Color(COLOR_BTNFACE); e.buttonTextColor = roclick::appearance::Color(COLOR_BTNTEXT);
        e.highlightColor = roclick::appearance::Color(COLOR_HIGHLIGHT); e.highlightTextColor = roclick::appearance::Color(COLOR_HIGHLIGHTTEXT);
        onState(std::move(e)); // Reporter only; scheduler never calls JS or waits for it.
      }
    });
  }
  ~ClickEngineModule() {
    roclick::Runtime::Shared().Stop();
    { std::lock_guard lock(mutex_); quit_ = true; }
    cv_.notify_one();
    if (reporter_.joinable()) reporter_.join();
  }
  REACT_METHOD(configure)
  void configure(ClickConfig c) noexcept {
    // Check doubles before conversion so fractional, NaN and huge JS values cannot truncate/overflow.
    if (!std::isfinite(c.intervalMs) || !std::isfinite(c.pressMs) || std::floor(c.intervalMs) != c.intervalMs ||
        std::floor(c.pressMs) != c.pressMs || c.intervalMs < 1 || c.intervalMs > 1000 || c.pressMs < 0 || c.pressMs >= c.intervalMs ||
        (c.button != "left" && c.button != "right" && c.button != "middle")) {
      roclick::Runtime::Shared().Configure({0,0,roclick::Button::Left,true}); return;
    }
    roclick::Runtime::Shared().Configure({int(c.intervalMs), int(c.pressMs),
      c.button == "right" ? roclick::Button::Right : c.button == "middle" ? roclick::Button::Middle : roclick::Button::Left, c.lockToTarget});
  }
  REACT_METHOD(start) void start() noexcept { roclick::Runtime::Shared().Start(); }
  REACT_METHOD(stop) void stop() noexcept { roclick::Runtime::Shared().Stop(); }
  REACT_METHOD(captureTarget) void captureTarget(double delayMs) noexcept {
    roclick::Runtime::Shared().CaptureAfter(std::isfinite(delayMs) ? int(std::clamp(delayMs, 500.0, 10000.0)) : 2000);
  }
  REACT_METHOD(clearTarget) void clearTarget() noexcept { roclick::Runtime::Shared().Clear(); }
  REACT_METHOD(setTheme) void setTheme(std::string value) noexcept {
    roclick::appearance::SetTheme(value == "dark" ? 2 : value == "light" ? 1 : 0);
  }
  REACT_METHOD(observe) void observe(bool enabled) noexcept { observing_ = enabled; pending_ = false; }
  REACT_METHOD(acknowledge) void acknowledge() noexcept { pending_ = false; }
 private:
  std::atomic<bool> observing_{false}, pending_{false};
  std::mutex mutex_;
  std::condition_variable cv_;
  bool quit_ = false;
  std::thread reporter_;
};
}
