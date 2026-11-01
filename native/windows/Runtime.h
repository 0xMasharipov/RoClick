#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "../core/Engine.h"
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace roclick {
struct RuntimeSnapshot {
  Snapshot engine;
  std::wstring targetTitle;
  DWORD targetPid = 0;
  double rate = 0;
  double stopLatencyMs = 0;
  bool highResolution = false;
  uint64_t revision = 0;
};
class Runtime final : private Platform {
 public:
  enum class TargetMode { Roblox, Foreground };
  static Runtime &Shared();
  explicit Runtime(bool installHotkeys = true, TargetMode targetMode = TargetMode::Roblox);
  ~Runtime();
  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;
  void Configure(Config config);
  void Start();
  void Stop();
  void Toggle();
  void CaptureAfter(int delayMs);
  void Clear();
  void Shutdown();
  bool PrepareForUpdate();
  RuntimeSnapshot Read();
  static Time Clock();
 private:
  enum class Op { Configure, Start, Stop, Toggle, Capture, Clear, Error };
  struct Command { Op op; Config config{}; int delay = 0; };
  void Enqueue(Command command);
  void Work();
  void Hooks();
  void Publish(bool force = true);
  static LRESULT CALLBACK Keyboard(int code, WPARAM message, LPARAM data);
  static void CALLBACK WindowEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
  Time Now() override { return Clock(); }
  TargetState Target() override;
  bool Capture() override;
  bool CaptureWindow(HWND window);
  static BOOL CALLBACK FindRobloxWindow(HWND window, LPARAM context);
  void ClearTarget() override;
  Submission Input(Button button, bool down, bool pair) override;
  Engine engine_{*this};
  HANDLE wake_ = nullptr, timer_ = nullptr, hooksReady_ = nullptr;
  std::thread worker_, hooks_;
  std::atomic<bool> shutdown_{false}, stopRequested_{false};
  std::atomic<bool> closingForUpdate_{false};
  std::atomic<bool> monitoring_{false};
  std::atomic<Time> stopAt_{0};
  std::atomic<DWORD> hookThreadId_{0};
  std::atomic<HWND> watched_{nullptr};
  std::atomic<bool> destroyed_{false};
  std::mutex commandsMutex_, snapshotMutex_;
  std::deque<Command> commands_;
  RuntimeSnapshot snapshot_;
  HWND target_ = nullptr;
  DWORD pid_ = 0, threadId_ = 0;
  HANDLE process_ = nullptr;
  std::wstring title_;
  bool highResolution_ = false;
  const TargetMode targetMode_;
  bool autoDetect_ = true;
  Time detectAt_ = 0;
  Time rateAt_ = 0;
  Time publishedAt_ = 0;
  State publishedState_ = State::Stopped;
  DWORD publishedPid_ = 0;
  uint64_t rateClicks_ = 0;
  double rate_ = 0, stopLatency_ = 0;
  static thread_local Runtime *hookOwner_;
  bool keys_[256]{};
};
} // namespace roclick
