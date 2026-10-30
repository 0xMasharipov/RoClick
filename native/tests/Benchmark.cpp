// Interactive Windows desktop only. Uses the SAME Runtime/Engine as the app.
// The receiver is a separate process: counts WM_*BUTTONUP independently of SendInput.
#include "Runtime.h"
#include <atomic>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace roclick;
struct Shared { volatile LONG64 received; volatile LONG64 downs; HWND window; };
static Shared *shared = nullptr;
static LRESULT CALLBACK Receive(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN) InterlockedIncrement64(&shared->downs);
  if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) InterlockedIncrement64(&shared->received);
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  if (message == WM_PAINT) {
    PAINTSTRUCT ps{}; auto dc = BeginPaint(hwnd, &ps);
    const wchar_t text[] = L"RoClick local input receiver — keep this window focused and the pointer inside.";
    TextOutW(dc, 20, 30, text, static_cast<int>(std::size(text)) - 1); EndPaint(hwnd, &ps); return 0;
  }
  return DefWindowProcW(hwnd, message, wp, lp);
}
static int Receiver(const wchar_t *mappingName) {
  HANDLE map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mappingName);
  if (!map) return 2;
  shared = static_cast<Shared *>(MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
  if (!shared) { CloseHandle(map); return 2; }
  WNDCLASSW wc{}; wc.lpfnWndProc = Receive; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"RoClickReceiver";
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  RegisterClassW(&wc);
  HWND window = CreateWindowW(wc.lpszClassName, L"RoClick local receiver", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    100,100,800,400,nullptr,nullptr,wc.hInstance,nullptr);
  if (!window) return 2;
  shared->window = window;
  MSG msg{}; while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
  UnmapViewOfFile(shared); CloseHandle(map); return 0;
}
static uint64_t CpuTicks() {
  FILETIME created{}, exit{}, kernel{}, user{}; GetProcessTimes(GetCurrentProcess(), &created,&exit,&kernel,&user);
  ULARGE_INTEGER k{}, u{}; k.LowPart=kernel.dwLowDateTime; k.HighPart=kernel.dwHighDateTime;
  u.LowPart=user.dwLowDateTime; u.HighPart=user.dwHighDateTime; return k.QuadPart+u.QuadPart;
}
int wmain(int argc, wchar_t **argv) {
  if (argc == 3 && std::wstring(argv[1]) == L"--receiver") return Receiver(argv[2]);
  const int seconds = argc > 1 ? _wtoi(argv[1]) : 60;
  const int loadThreads = argc > 2 ? _wtoi(argv[2]) : 0;
  if (seconds < 1 || seconds > 3600 || loadThreads < 0 || loadThreads > 256) return 2;
  const auto mappingName = L"Local\\RoClickBenchmark-" + std::to_wstring(GetCurrentProcessId());
  HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),mappingName.c_str());
  if (!map) return 2;
  shared=static_cast<Shared *>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
  if (!shared) { CloseHandle(map); return 2; }
  wchar_t exe[32768]{}; GetModuleFileNameW(nullptr,exe,32768);
  std::wstring command = L"\"" + std::wstring(exe) + L"\" --receiver " + mappingName;
  STARTUPINFOW startup{sizeof(startup)}; PROCESS_INFORMATION process{};
  if (!CreateProcessW(exe,command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)) return 2;
  AllowSetForegroundWindow(process.dwProcessId);
  for (int i=0; i<500 && !shared->window; ++i) Sleep(10);
  if (!shared->window) { std::cerr << "Receiver did not open.\n"; return 2; }
  SetForegroundWindow(shared->window);
  RECT rect{}; GetWindowRect(shared->window,&rect); SetCursorPos(rect.left+100,rect.top+120);
  Runtime runtime(false);
  runtime.CaptureAfter(500);
  Sleep(750);
  if (!runtime.Read().targetPid) { std::cerr << "Capture failed: requires an unlocked interactive desktop and foreground receiver.\n"; PostMessageW(shared->window,WM_CLOSE,0,0); return 2; }
  std::atomic<bool> load{true};
  std::vector<std::thread> loaders;
  for (int n=0;n<loadThreads;++n) loaders.emplace_back([&load] { volatile double v=1; while(load.load(std::memory_order_relaxed)) { v=std::sqrt(v+2.0); } });
  bool passed = true;
  std::cout << "{\"schema\":1,\"secondsPerRate\":" << seconds << ",\"loadThreads\":" << loadThreads << ",\"samples\":[\n";
  bool first=true;
  for (const int cps : {10,100,500,1000}) {
    InterlockedExchange64(&shared->received,0); InterlockedExchange64(&shared->downs,0);
    runtime.Configure({1000/cps,0,Button::Left,true});
    const auto cpu = CpuTicks(); const auto wall=Runtime::Clock();
    runtime.Start();
    while(Runtime::Clock()-wall < Time(seconds)*1'000'000'000) Sleep(1);
    const auto stop = Runtime::Clock(); runtime.Stop();
    for (int i=0;i<1000 && runtime.Read().engine.ended<stop;++i) Sleep(1);
    const auto result=runtime.Read(); const auto &s=result.engine;
    const double elapsed=double(s.ended-s.started)/1e9;
    const double rate=elapsed>0 ? double(s.clicks)/elapsed : 0;
    const double cpuPercent=double(CpuTicks()-cpu)/1e7 / (double(Runtime::Clock()-wall)/1e9)*100;
    Sleep(100); // Let the independent receiver drain already-submitted messages.
    const bool ok=std::abs(rate-cps)/cps<=0.05 && result.stopLatencyMs<50 && !s.held && !s.failures && s.ended>=stop;
    passed &= ok;
    if(!first) std::cout << ",\n"; first=false;
    std::cout << "{\"configuredCps\":" << cps << ",\"elapsedSeconds\":" << elapsed
      << ",\"submitted\":" << s.clicks << ",\"received\":" << shared->received
      << ",\"receiverDowns\":" << shared->downs << ",\"submissionRate\":" << rate
      << ",\"cpuPercentOneCore\":" << cpuPercent << ",\"stopMs\":" << result.stopLatencyMs
      << ",\"gapMeanMs\":" << s.gapMeanMs << ",\"gapStdDevMs\":" << (s.gapCount>1 ? std::sqrt(s.gapM2/double(s.gapCount-1)) : 0)
      << ",\"gapMinMs\":" << s.gapMinMs << ",\"gapMaxMs\":" << s.gapMaxMs
      << ",\"skipped\":" << s.skipped << ",\"failures\":" << s.failures
      << ",\"highResolution\":" << (result.highResolution?"true":"false") << ",\"passesTarget\":" << (ok?"true":"false") << "}";
  }
  std::cout << "\n],\"passesAllTargets\":" << (passed?"true":"false") << "}\n";
  load=false; for(auto &thread:loaders) thread.join(); runtime.Shutdown();
  PostMessageW(shared->window,WM_CLOSE,0,0);
  WaitForSingleObject(process.hProcess,5000); CloseHandle(process.hThread); CloseHandle(process.hProcess);
  UnmapViewOfFile(shared); CloseHandle(map);
  return passed ? 0 : 1;
}
