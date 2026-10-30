#include "Runtime.h"
#include <stdexcept>
#include <utility>

namespace roclick {
thread_local Runtime *Runtime::hookOwner_ = nullptr;
Runtime &Runtime::Shared() { static Runtime runtime; return runtime; }
Time Runtime::Clock() {
  static const auto frequency = [] { LARGE_INTEGER v{}; QueryPerformanceFrequency(&v); return v.QuadPart; }();
  LARGE_INTEGER value{}; QueryPerformanceCounter(&value);
  // Split to avoid overflowing the intermediate after a few minutes of uptime.
  return (value.QuadPart / frequency) * 1'000'000'000 +
    (value.QuadPart % frequency) * 1'000'000'000 / frequency;
}
Runtime::Runtime(bool installHotkeys) {
  wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  hooksReady_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  highResolution_ = timer_ != nullptr;
  if (!timer_) timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
  if (!wake_ || !timer_ || !hooksReady_) {
    if (wake_) CloseHandle(wake_);
    if (timer_) CloseHandle(timer_);
    if (hooksReady_) CloseHandle(hooksReady_);
    throw std::runtime_error("Could not create native scheduler handles.");
  }
  worker_ = std::thread([this] { Work(); });
  // Window lifetime hook is needed even for benchmark runs without hotkeys.
  hooks_ = std::thread([this, installHotkeys] {
    hookOwner_ = this;
    if (!installHotkeys) keys_[0] = true;
    Hooks();
  });
  WaitForSingleObject(hooksReady_, INFINITE);
}
Runtime::~Runtime() { Shutdown(); CloseHandle(wake_); CloseHandle(timer_); CloseHandle(hooksReady_); }
void Runtime::Shutdown() {
  if (shutdown_.exchange(true)) return;
  stopRequested_ = true;
  SetEvent(wake_);
  const auto id = hookThreadId_.load();
  if (id) PostThreadMessageW(id, WM_QUIT, 0, 0);
  if (hooks_.joinable()) hooks_.join();
  if (worker_.joinable()) worker_.join();
}
void Runtime::Enqueue(Command command) {
  if (shutdown_) return;
  {
    std::lock_guard lock(commandsMutex_);
    if (command.op == Op::Configure) {
      // Coalesce slider updates; queue memory cannot grow with a stalled worker.
      for (auto &pending : commands_) if (pending.op == Op::Configure) { pending = command; SetEvent(wake_); return; }
    }
    if (commands_.size() >= 32) return;
    commands_.push_back(command);
  }
  SetEvent(wake_);
}
void Runtime::Configure(Config c) { Enqueue({Op::Configure, c}); }
void Runtime::Start() { Enqueue({Op::Start}); }
void Runtime::Toggle() { Enqueue({Op::Toggle}); }
void Runtime::CaptureAfter(int delay) { Enqueue({Op::Capture, {}, delay}); }
void Runtime::Clear() { Enqueue({Op::Clear}); }
void Runtime::Stop() {
  // Stop outranks queued starts/configuration and interrupts a press wait.
  std::lock_guard lock(commandsMutex_);
  commands_.clear();
  stopAt_ = Clock();
  stopRequested_ = true;
  SetEvent(wake_);
}
RuntimeSnapshot Runtime::Read() { std::lock_guard lock(snapshotMutex_); return snapshot_; }
void Runtime::Work() {
  rateAt_ = Clock();
  while (!shutdown_) {
    std::deque<Command> pending;
    bool stop;
    {
      std::lock_guard lock(commandsMutex_);
      stop = stopRequested_.exchange(false);
      pending.swap(commands_);
    }
    if (stop) { engine_.Stop(); stopLatency_ = double(Clock() - stopAt_) / Millisecond; }
    for (const auto &c : pending) {
      if (stopRequested_ || shutdown_) break;
      switch (c.op) {
        case Op::Configure: engine_.Configure(c.config); break;
        case Op::Start: engine_.Start(); break;
        case Op::Stop: engine_.Stop(); break;
        case Op::Toggle: if (engine_.Active()) engine_.Stop(); else engine_.Start(); break;
        case Op::Capture: engine_.CaptureAfter(c.delay); break;
        case Op::Clear: engine_.ClearTarget(); break;
        case Op::Error: engine_.ExternalFailure("Native keyboard/window monitoring could not start. Restart RoClick on the interactive desktop."); break;
      }
    }
    if (!stopRequested_ && !shutdown_) engine_.Tick();
    Publish();
    const auto remaining = std::max<Time>(100, engine_.NextWake() - Clock());
    LARGE_INTEGER due{}; due.QuadPart = -((remaining + 99) / 100);
    if (!SetWaitableTimerEx(timer_, &due, 0, nullptr, nullptr, nullptr, 0)) {
      engine_.ExternalFailure("Windows could not arm the scheduler timer. Restart RoClick.", GetLastError());
      Publish(); WaitForSingleObject(wake_, 100); continue;
    }
    HANDLE handles[] = {wake_, timer_};
    if (WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_FAILED) {
      engine_.ExternalFailure("Windows scheduler wait failed. Restart RoClick.", GetLastError());
      Publish(); WaitForSingleObject(wake_, 100);
    }
  }
  engine_.Stop(); // Releases the actual held button, not the current selection.
  Publish();
  ClearTarget();
}
void Runtime::Publish() {
  const auto now = Clock();
  const auto &s = engine_.Get();
  if (s.clicks < rateClicks_ || s.started > rateAt_) { rateAt_ = now; rateClicks_ = s.clicks; rate_ = 0; }
  if (now - rateAt_ >= 250 * Millisecond) {
    rate_ = double(s.clicks - rateClicks_) * 1'000'000'000.0 / double(now - rateAt_);
    rateClicks_ = s.clicks; rateAt_ = now;
  }
  std::lock_guard lock(snapshotMutex_);
  snapshot_ = {s, title_, pid_, rate_, stopLatency_, highResolution_};
}
bool Runtime::Capture() {
  ClearTarget();
  HWND window = GetAncestor(GetForegroundWindow(), GA_ROOT);
  DWORD pid = 0;
  const DWORD thread = GetWindowThreadProcessId(window, &pid);
  if (!window || !thread || pid == GetCurrentProcessId() || window == GetDesktopWindow() || window == GetShellWindow()) return false;
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
  if (!process) return false;
  FILETIME created{}, exit{}, kernel{}, user{};
  if (!GetProcessTimes(process, &created, &exit, &kernel, &user)) { CloseHandle(process); return false; }
  destroyed_ = false;
  watched_ = window;
  DWORD verifiedPid = 0;
  if (GetWindowThreadProcessId(window, &verifiedPid) != thread || verifiedPid != pid || WaitForSingleObject(process, 0) != WAIT_TIMEOUT) {
    watched_ = nullptr; CloseHandle(process); return false;
  }
  target_ = window; pid_ = pid; threadId_ = thread; process_ = process; created_ = created;
  wchar_t title[512]{}; GetWindowTextW(window, title, 512);
  title_ = title[0] ? title : L"Untitled window";
  return true;
}
void Runtime::ClearTarget() {
  watched_ = nullptr;
  if (process_) CloseHandle(process_);
  process_ = nullptr; target_ = nullptr; pid_ = 0; threadId_ = 0; title_.clear();
}
TargetState Runtime::Target() {
  if (!target_) return TargetState::Missing;
  DWORD pid = 0;
  const DWORD thread = GetWindowThreadProcessId(target_, &pid);
  FILETIME created{}, exit{}, kernel{}, user{};
  if (destroyed_ || !IsWindow(target_) || pid != pid_ || thread != threadId_ ||
      WaitForSingleObject(process_, 0) != WAIT_TIMEOUT ||
      !GetProcessTimes(process_, &created, &exit, &kernel, &user) ||
      CompareFileTime(&created_, &created) != 0) return TargetState::Destroyed;
  return GetAncestor(GetForegroundWindow(), GA_ROOT) == target_ ? TargetState::Focused : TargetState::Background;
}
Submission Runtime::Input(Button button, bool down, bool pair) {
  // Narrow the stop/focus race again immediately before submitting a down.
  if (down && (stopRequested_ || shutdown_ ||
      (engine_.Get().config.lockToTarget && Target() != TargetState::Focused))) return {0, ERROR_CANCELLED, true};
  const DWORD downs[] = {MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_MIDDLEDOWN};
  const DWORD ups[] = {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP};
  const auto index = static_cast<unsigned>(button);
  INPUT inputs[2]{};
  inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = down ? downs[index] : ups[index];
  inputs[1].type = INPUT_MOUSE; inputs[1].mi.dwFlags = ups[index];
  SetLastError(ERROR_SUCCESS);
  const UINT sent = SendInput(pair ? 2 : 1, inputs, sizeof(INPUT));
  return {sent, sent == (pair ? 2u : 1u) ? 0u : GetLastError()};
}
void Runtime::Hooks() {
  MSG message{}; PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
  hookThreadId_ = GetCurrentThreadId();
  const auto keyboard = keys_[0] ? nullptr : SetWindowsHookExW(WH_KEYBOARD_LL, Keyboard, GetModuleHandleW(nullptr), 0);
  const auto events = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, WindowEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
  if ((!keys_[0] && !keyboard) || !events) Enqueue({Op::Error});
  SetEvent(hooksReady_);
  while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  if (keyboard) UnhookWindowsHookEx(keyboard);
  if (events) UnhookWinEvent(events);
  hookOwner_ = nullptr;
}
LRESULT CALLBACK Runtime::Keyboard(int code, WPARAM message, LPARAM data) {
  auto *self = hookOwner_;
  if (code == HC_ACTION && self) {
    const auto *key = reinterpret_cast<KBDLLHOOKSTRUCT *>(data);
    if (!(key->flags & LLKHF_INJECTED) && key->vkCode < 256) {
      const auto vk = key->vkCode;
      const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
      const bool first = down && !self->keys_[vk];
      self->keys_[vk] = down;
      if (first) {
        DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
        const bool editing = pid == GetCurrentProcessId();
        // Digits remain ordinary editable keys in RoClick. They pass through
        // everywhere; Escape and F6 remain global, including our own window.
        if (vk == VK_ESCAPE || (!editing && (vk == '2' || vk == VK_NUMPAD2))) self->Stop();
        else if (vk == VK_F6) self->Toggle();
        else if (!editing && (vk == '1' || vk == VK_NUMPAD1)) self->Start();
      }
    }
  }
  return CallNextHookEx(nullptr, code, message, data);
}
void CALLBACK Runtime::WindowEvent(HWINEVENTHOOK, DWORD, HWND window, LONG object, LONG child, DWORD, DWORD) {
  auto *self = hookOwner_;
  if (self && object == OBJID_WINDOW && child == CHILDID_SELF && window == self->watched_) {
    self->destroyed_ = true; SetEvent(self->wake_);
  }
}
} // namespace roclick
