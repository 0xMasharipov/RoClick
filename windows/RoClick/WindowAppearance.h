#pragma once
#include <atomic>
#include <cstdio>
#include <string>
namespace roclick::appearance {
inline std::atomic<HWND> window{nullptr};
inline std::atomic<int> theme{0}; // 0 system, 1 light, 2 dark
inline std::atomic<bool> acrylic{false}, dark{false}, highContrast{false};
constexpr UINT ThemeMessage = WM_APP + 41;
void Install(HWND hwnd);
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
