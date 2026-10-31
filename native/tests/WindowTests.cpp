#include "Runtime.h"
#include <cstdlib>
#include <iostream>

using namespace roclick;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << "\n"; std::exit(1); } } while(false)

static LRESULT CALLBACK Receiver(HWND window, UINT message, WPARAM w, LPARAM l) {
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(window, message, w, l);
}

// A real top-level window in an independent process, without requiring Roblox in CI.
static int RunReceiver() {
  WNDCLASSW cls{};
  cls.lpfnWndProc = Receiver; cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"RoClickTestReceiver";
  if (!RegisterClassW(&cls)) return 2;
  HWND window = CreateWindowExW(0, cls.lpszClassName, L"Game session", WS_OVERLAPPEDWINDOW,
    10, 10, 320, 240, nullptr, nullptr, cls.hInstance, nullptr);
  if (!window) return 2;
  ShowWindow(window, SW_SHOWNOACTIVATE);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  return 0;
}

struct Child {
  PROCESS_INFORMATION info{};
  Child() {
    wchar_t path[32768]{}; CHECK(GetModuleFileNameW(nullptr, path, 32768));
    std::wstring command = L"\"" + std::wstring(path) + L"\" --receiver";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    CHECK(CreateProcessW(path, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info));
  }
  ~Child() { TerminateProcess(info.hProcess, 0); WaitForSingleObject(info.hProcess, 3000); CloseHandle(info.hThread); CloseHandle(info.hProcess); }
};

template<class Predicate> static bool Wait(Predicate predicate) {
  for (int attempt = 0; attempt < 500; ++attempt) { if (predicate()) return true; Sleep(10); }
  return false;
}

int wmain(int argc, wchar_t **argv) {
  if (argc == 2 && std::wstring(argv[1]) == L"--receiver") return RunReceiver();
  Runtime runtime(false);
  Sleep(1100);
  CHECK(runtime.Read().targetPid == 0); CHECK(runtime.Read().engine.failures == 0);
  {
    Child child;
    CHECK(Wait([&] { return runtime.Read().targetPid == child.info.dwProcessId; }));
    CHECK(runtime.Read().targetTitle == L"Game session");
    CHECK(runtime.Read().engine.state == State::Stopped); CHECK(runtime.Read().engine.clicks == 0);
    runtime.Clear();
    CHECK(Wait([&] { return runtime.Read().targetPid == 0; }));
    Sleep(1200); CHECK(runtime.Read().targetPid == 0); // Clear suspends idle discovery.
    runtime.CaptureAfter(0);
    CHECK(Wait([&] { return runtime.Read().targetPid == child.info.dwProcessId; }));
  }
  CHECK(Wait([&] { return runtime.Read().engine.errorKind == ErrorKind::TargetClosed; }));
  {
    Child child;
    CHECK(Wait([&] { return runtime.Read().targetPid == child.info.dwProcessId; }));
    CHECK(runtime.Read().engine.state == State::Stopped);
    CHECK(runtime.Read().engine.errorKind == ErrorKind::None); CHECK(runtime.Read().engine.clicks == 0);
  }
  runtime.Shutdown();
  std::cout << "Windows discovery passed: late launch, executable identity, clear, immediate retry, restart without clicking.\n";
}
