#include "pch.h"
#include "WindowAppearance.h"
#include "../../native/windows/Runtime.h"
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
}
static LRESULT CALLBACK Subclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
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
  if (msg == WM_NCDESTROY) { window = nullptr; RemoveWindowSubclass(hwnd, Subclass, 1); }
  return DefSubclassProc(hwnd, msg, wp, lp);
}
void Install(HWND hwnd) { window = hwnd; SetWindowSubclass(hwnd, Subclass, 1, 0); Update(hwnd); }
}
