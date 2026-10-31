#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <chrono>
#include <mutex>
#include <string>

namespace roclick {
struct UpdateSnapshot {
  bool enabled = true;
  std::string state = "unavailable";
  std::string version;
  std::string availableVersion;
};

class UpdateService {
 public:
  static UpdateService &Shared();
  void Initialize();
  UpdateSnapshot Read();
  bool SetEnabled(bool enabled);
  ~UpdateService();
 private:
  void Launch();
  std::mutex mutex_;
  UpdateSnapshot snapshot_;
  std::wstring directory_, cache_, job_;
  HANDLE helper_ = nullptr;
  bool initialized_ = false;
  std::chrono::steady_clock::time_point launched_{};
};
} // namespace roclick
