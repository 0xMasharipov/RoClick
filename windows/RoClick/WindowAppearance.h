#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
namespace roclick::appearance {
inline std::atomic<HWND> window{nullptr};
inline std::atomic<int> theme{0}; // 0 system, 1 light, 2 dark
inline std::atomic<bool> acrylic{false}, dark{false}, highContrast{false};
inline std::atomic<uint64_t> revision{0};
constexpr UINT ThemeMessage = WM_APP + 41;
constexpr UINT UpdateAttentionMessage = WM_APP + 42;
inline UINT InstallUpdateMessage() { return RegisterWindowMessageW(L"RoClick.InstallVerifiedUpdate"); }
void Install(HWND hwnd);
bool CloseForUpdate();
void RequestUpdateAttention();
inline void SetTheme(int value) {
  theme = value;
  if (const auto hwnd = window.load()) PostMessageW(hwnd, ThemeMessage, 0, 0);
}
inline std::string Color(int index) {
  const COLORREF c = GetSysColor(index);
  char text[8]; snprintf(text, sizeof(text), "#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
  return text;
}
}
