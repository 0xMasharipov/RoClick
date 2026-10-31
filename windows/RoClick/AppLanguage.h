#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace roclick::language {
inline constexpr wchar_t SettingsKey[] = L"Software\\RoClick";
inline std::string Get() {
  wchar_t value[8]{};
  DWORD size = sizeof(value);
  if (RegGetValueW(HKEY_CURRENT_USER, SettingsKey, L"Language", RRF_RT_REG_SZ,
      nullptr, value, &size) == ERROR_SUCCESS) {
    if (std::wstring(value) == L"tr") return "tr";
    if (std::wstring(value) == L"en") return "en";
  }
  ULONG count = 0, length = 0;
  if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &length) && length) {
    std::vector<wchar_t> languages(length);
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, languages.data(), &length)) {
      const std::wstring first(languages.data());
      if (first == L"tr" || first.rfind(L"tr-", 0) == 0) return "tr";
    }
  }
  return "en";
}
inline bool Set(std::string const &value) {
  if (value != "en" && value != "tr") return false;
  const wchar_t *text = value == "tr" ? L"tr" : L"en";
  return RegSetKeyValueW(HKEY_CURRENT_USER, SettingsKey, L"Language", REG_SZ,
    text, 3 * sizeof(wchar_t)) == ERROR_SUCCESS;
}
}
