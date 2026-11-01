#include "pch.h"
#include "WindowAppearance.h"
#include "../../native/windows/Runtime.h"
#include "../../native/windows/UpdateService.h"
#include "AppLanguage.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace roclick::appearance {
static void Update(HWND hwnd) {
  HIGHCONTRASTW hc{sizeof(hc)};
  SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0);
  highContrast = (hc.dwFlags & HCF_HIGHCONTRASTON) != 0;
  bool systemDark = false;
  try {
    const auto color = winrt::Windows::UI::ViewManagement::UISettings().GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Background);
    systemDark = int(color.R) + color.G + color.B < 384;
  } catch (...) { /* A solid light fallback is always available. */ }
  dark = theme == 2 || (theme == 0 && systemDark);
  DWORD transparency = 0, size = sizeof(transparency);
  RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &transparency, &size);
  BOOL composition = FALSE;
  DwmIsCompositionEnabled(&composition);
  const bool permitted = transparency != 0 && !highContrast && composition && !GetSystemMetrics(SM_REMOTESESSION);
  // Documented DWM Desktop Acrylic (Windows 11 build 22621+). Windows 10,
  // disabled transparency, high contrast and remote sessions use solid fills.
  const DWM_SYSTEMBACKDROP_TYPE backdrop = permitted ? DWMSBT_TRANSIENTWINDOW : DWMSBT_NONE;
  const auto result = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
  acrylic = permitted && SUCCEEDED(result);
  MARGINS margins = acrylic ? MARGINS{-1,-1,-1,-1} : MARGINS{0,0,0,0};
  DwmExtendFrameIntoClientArea(hwnd, &margins);
  const BOOL isDark = dark && !highContrast;
  DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDark, sizeof(isDark));
  InvalidateRect(hwnd, nullptr, TRUE);
  ++revision;
}
static LRESULT CALLBACK Subclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
  if (msg == UpdateAttentionMessage) {
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    const auto popup = GetLastActivePopup(hwnd);
    if (!SetForegroundWindow(IsWindowVisible(popup) ? popup : hwnd)) {
      FLASHWINFO flash{sizeof(flash), hwnd, FLASHW_TRAY | FLASHW_TIMERNOFG, 3, 0};
      FlashWindowEx(&flash);
    }
    return 0;
  }
  if (msg == InstallUpdateMessage()) {
    if (UpdateService::Shared().Read().enabled) {
      if (Runtime::Shared().PrepareForUpdate()) PostMessageW(hwnd, WM_CLOSE, 0, 0);
      else MessageBoxW(hwnd, language::Get() == "tr"
        ? L"Tıklama motoru güvenle durdurulamadı. Durdur’a basıp güncellemeyi yeniden deneyin."
        : L"The click engine could not stop safely. Press Stop and try the update again.", L"RoClick", MB_OK | MB_ICONERROR);
    }
    return 0;
  }
  if (msg == WM_SETTINGCHANGE || msg == WM_THEMECHANGED || msg == WM_DWMCOMPOSITIONCHANGED || msg == ThemeMessage) Update(hwnd);
  if (msg == WM_GETMINMAXINFO) {
    const UINT dpi = GetDpiForWindow(hwnd);
    auto *limits = reinterpret_cast<MINMAXINFO *>(lp);
    limits->ptMinTrackSize = {MulDiv(360, static_cast<int>(dpi), 96), MulDiv(480, static_cast<int>(dpi), 96)};
    return 0;
  }
  // Stop before RN's unload waits on JavaScript, including a busy JS thread.
  if (msg == WM_CLOSE || msg == WM_QUERYENDSESSION) Runtime::Shared().Stop();
  if (msg == WM_ENDSESSION && wp) Runtime::Shared().Shutdown();
  if (msg == WM_NCDESTROY) { RemovePropW(hwnd, L"RoClick.UpdateWindow"); window = nullptr; RemoveWindowSubclass(hwnd, Subclass, 1); }
  return DefSubclassProc(hwnd, msg, wp, lp);
}
void Install(HWND hwnd) {
  window = hwnd; SetPropW(hwnd, L"RoClick.UpdateWindow", reinterpret_cast<HANDLE>(1));
  SetWindowSubclass(hwnd, Subclass, 1, 0); Update(hwnd);
}
void RequestUpdateAttention() {
  if (const auto hwnd = window.load()) PostMessageW(hwnd, UpdateAttentionMessage, 0, 0);
}
bool CloseForUpdate() {
  const auto update = UpdateService::Shared().Read();
  if (!update.enabled || update.state != "ready" || update.availableVersion.empty()) return false;
  struct Context { std::wstring executable; bool posted = false; } context;
  wchar_t path[32768]{};
  const auto length = GetModuleFileNameW(nullptr, path, 32768);
  if (!length || length >= 32768 || !InstallUpdateMessage()) return false;
  context.executable.assign(path, length);
  EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
    auto &c = *reinterpret_cast<Context *>(data);
    if (!GetPropW(hwnd, L"RoClick.UpdateWindow")) return TRUE;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return TRUE;
    wchar_t executable[32768]{}; DWORD size = 32768;
    const bool same = QueryFullProcessImageNameW(process, 0, executable, &size) &&
      _wcsicmp(executable, c.executable.c_str()) == 0;
    CloseHandle(process);
    if (same && PostMessageW(hwnd, InstallUpdateMessage(), 0, 0)) c.posted = true;
    return TRUE;
  }, reinterpret_cast<LPARAM>(&context));
  return context.posted;
}
}
