#pragma once
#include <initializer_list>
#include <string_view>

namespace roclick {
// Match the executable, never the title: browsers and launchers can also say Roblox.
inline bool IsRobloxPlayer(std::wstring_view path) {
  const auto separator = path.find_last_of(L"\\/");
  const auto name = path.substr(separator == path.npos ? 0 : separator + 1);
  for (const auto expected : {L"robloxplayerbeta.exe", L"robloxplayer.exe", L"windows10universal.exe"}) {
    const std::wstring_view candidate(expected);
    if (name.size() != candidate.size()) continue;
    bool matches = true;
    for (size_t i = 0; i < name.size(); ++i) {
      const auto lower = name[i] >= L'A' && name[i] <= L'Z' ? name[i] + (L'a' - L'A') : name[i];
      if (lower != candidate[i]) { matches = false; break; }
    }
    if (matches) return true;
  }
  return false;
}
} // namespace roclick
