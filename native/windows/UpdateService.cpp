#include "UpdateService.h"
#include <shlobj.h>
#include <algorithm>
#include <cstdint>
#include <string_view>

namespace roclick {
namespace {
constexpr wchar_t SettingsKey[] = L"Software\\RoClick";
bool Enabled() {
  DWORD value = 1, size = sizeof(value);
  return RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"AutomaticUpdates", RRF_RT_REG_DWORD,
    nullptr, &value, &size) != ERROR_SUCCESS || value != 0;
}
std::string ReadText(std::wstring const &path, DWORD limit) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return {};
  std::string text(limit, '\0');
  DWORD count = 0;
  const bool ok = ReadFile(file, text.data(), limit, &count, nullptr) != FALSE;
  CloseHandle(file);
  if (!ok || count == limit) return {};
  text.resize(count);
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
  return text;
}
// CommandLineToArgvW rules: escape quotes and trailing backslashes inside quotes.
std::wstring Quote(std::wstring_view text) {
  std::wstring result = L"\"";
  size_t slashes = 0;
  for (const auto character : text) {
    if (character == L'\\') { ++slashes; continue; }
    result.append(character == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    result += character; slashes = 0;
  }
  result.append(slashes * 2, L'\\'); result += L'"';
  return result;
}
}

UpdateService &UpdateService::Shared() { static UpdateService service; return service; }
UpdateService::~UpdateService() {
  // The helper intentionally outlives us to install only after process exit.
  if (helper_) CloseHandle(helper_);
}

void UpdateService::Initialize() {
  std::lock_guard lock(mutex_);
  if (initialized_) return;
  wchar_t executable[32768]{};
  const auto length = GetModuleFileNameW(nullptr, executable, 32768);
  if (!length || length >= 32768) return;
  directory_ = std::wstring(executable, length);
  directory_.resize(directory_.find_last_of(L"\\/"));
  snapshot_.version = ReadText(directory_ + L"\\version.txt", 64);
  if (snapshot_.version.empty() || snapshot_.version.find_first_not_of("0123456789.") != std::string::npos) return;
  PWSTR local = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return;
  cache_ = std::wstring(local) + L"\\RoClick\\Updates\\";
  CoTaskMemFree(local);
  // Stable per-installation directory, shared across versions and app instances.
  uint64_t hash = 14695981039346656037ull;
  for (auto ch : directory_) {
    if (ch >= L'A' && ch <= L'Z') ch += L'a' - L'A';
    hash = (hash ^ static_cast<uint64_t>(ch)) * 1099511628211ull;
  }
  cache_ += std::to_wstring(hash);
  const auto result = SHCreateDirectoryExW(nullptr, cache_.c_str(), nullptr);
  if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS) return;
  initialized_ = true;
  Launch();
}

void UpdateService::Launch() {
  launched_ = std::chrono::steady_clock::now();
  snapshot_.state = "error";
  snapshot_.availableVersion.clear();
  if (helper_) { CloseHandle(helper_); helper_ = nullptr; }
  if (!job_.empty()) {
    for (const auto name : {L"update.ps1", L"state.txt", L"state.tmp", L"release.json", L"SHA256SUMS.txt", L"RoClick-Setup.exe"})
      DeleteFileW((job_ + L"\\" + name).c_str());
    RemoveDirectoryW(job_.c_str());
  }
  job_ = cache_ + L"\\" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
  if (!CreateDirectoryW(job_.c_str(), nullptr)) return;
  const auto script = job_ + L"\\update.ps1";
  if (!CopyFileW((directory_ + L"\\update.ps1").c_str(), script.c_str(), TRUE)) return;
  wchar_t system[32768]{};
  const auto length = GetSystemDirectoryW(system, 32768);
  if (!length || length >= 32768) return;
  const auto powershell = std::wstring(system, length) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return;
  ULARGE_INTEGER started{}; started.LowPart = created.dwLowDateTime; started.HighPart = created.dwHighDateTime;
  const std::wstring version(snapshot_.version.begin(), snapshot_.version.end());
  auto command = Quote(powershell) + L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " + Quote(script) +
    L" -AppDirectory " + Quote(directory_) + L" -CurrentVersion " + Quote(version) +
    L" -ParentProcessId " + std::to_wstring(GetCurrentProcessId()) +
    L" -ParentStarted " + std::to_wstring(started.QuadPart) + L" -StateDirectory " + Quote(job_);
  STARTUPINFOW startup{}; startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
      nullptr, job_.c_str(), &startup, &process)) return;
  CloseHandle(process.hThread); helper_ = process.hProcess;
  snapshot_.state = "checking";
}

UpdateSnapshot UpdateService::Read() {
  std::lock_guard lock(mutex_);
  snapshot_.enabled = Enabled();
  if (initialized_) {
    const bool exited = helper_ && WaitForSingleObject(helper_, 0) == WAIT_OBJECT_0;
    // Another instance may have owned the lock and since closed. Retry rather
    // than leaving this window permanently without a helper.
    if ((!helper_ || exited) && std::chrono::steady_clock::now() - launched_ >= std::chrono::minutes(1)) Launch();
    const auto status = ReadText(job_ + L"\\state.txt", 256);
    const auto newline = status.find('\n');
    const auto phase = status.substr(0, newline);
    for (const auto allowed : {"checking", "current", "downloading", "ready", "error", "disabled", "otherInstance"}) {
      if (phase == allowed) {
        snapshot_.state = phase;
        snapshot_.availableVersion = newline == status.npos ? "" : status.substr(newline + 1);
        break;
      }
    }
  }
  auto result = snapshot_;
  if (!result.enabled) result.state = "disabled";
  return result;
}

bool UpdateService::SetEnabled(bool enabled) {
  const DWORD value = enabled ? 1 : 0;
  return RegSetKeyValueW(HKEY_CURRENT_USER, SettingsKey, L"AutomaticUpdates", REG_DWORD, &value, sizeof(value)) == ERROR_SUCCESS;
}
} // namespace roclick
