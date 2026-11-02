// RoClick.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "RoClick.h"

#include "AutolinkedNativeModules.g.h"

#include "NativeModules.h"
#include "ClickEngineModule.h"
#include "AppSettingsModule.h"
#include "WindowAppearance.h"
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <mutex>

// Opt-in local diagnostics for installed release validation. No logs are
// written unless the launching environment supplies an explicit file path.
struct ValidationErrors : winrt::implements<ValidationErrors, winrt::Microsoft::ReactNative::IRedBoxHandler> {
  explicit ValidationErrors(std::filesystem::path path) : path_(std::move(path)) {}
  bool IsDevSupportEnabled() noexcept { return true; }
  void ShowNewError(winrt::Microsoft::ReactNative::IRedBoxErrorInfo const &info,
      winrt::Microsoft::ReactNative::RedBoxErrorType) noexcept { Write(info); }
  void UpdateError(winrt::Microsoft::ReactNative::IRedBoxErrorInfo const &info) noexcept { Write(info); }
  void DismissRedBox() noexcept {}
 private:
  void Write(winrt::Microsoft::ReactNative::IRedBoxErrorInfo const &info) noexcept {
    try {
      std::lock_guard lock(mutex_);
      if (std::filesystem::exists(path_) && std::filesystem::file_size(path_) > 1'000'000) return;
      std::ofstream log(path_, std::ios::app);
      log << winrt::to_string(info.Message()) << '\n' << winrt::to_string(info.ComponentStack()) << '\n';
      for (auto const &frame : info.Callstack())
        log << winrt::to_string(frame.Method()) << " at " << winrt::to_string(frame.File())
            << ':' << frame.Line() << ':' << frame.Column() << '\n';
    } catch (...) {} // Diagnostics cannot interrupt input or app shutdown.
  }
  std::filesystem::path path_;
  std::mutex mutex_;
};

// A PackageProvider containing any turbo modules you define within this app project
struct CompReactPackageProvider
    : winrt::implements<CompReactPackageProvider, winrt::Microsoft::ReactNative::IReactPackageProvider> {
 public: // IReactPackageProvider
  void CreatePackage(winrt::Microsoft::ReactNative::IReactPackageBuilder const &packageBuilder) noexcept {
    AddAttributedModules(packageBuilder, true);
  }
};

// The entry point of the Win32 application
static int RunApp() {
  // The installer checks this from the beginning of startup, including while
  // React Native is loading. Multiple app windows retain their own handles.
  struct ApplicationLifetime {
    HANDLE mutex = nullptr;
    ApplicationLifetime() {
      HANDLE gate = CreateMutexW(nullptr, FALSE, L"Local\\RoClick.Installation");
      if (!gate) throw std::runtime_error("Could not open the installation gate.");
      const auto result = WaitForSingleObject(gate, INFINITE);
      if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
        mutex = CreateMutexW(nullptr, FALSE, L"Local\\RoClick.Application");
        ReleaseMutex(gate);
      }
      CloseHandle(gate);
      if (!mutex) throw std::runtime_error("Could not register the application instance.");
    }
    ~ApplicationLifetime() { if (mutex) CloseHandle(mutex); }
  } application;
  // Initialize WinRT
  winrt::init_apartment(winrt::apartment_type::single_threaded);

  // Enable per monitor DPI scaling
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  // Find the path hosting the app exe file
  WCHAR appDirectory[MAX_PATH];
  GetModuleFileNameW(NULL, appDirectory, MAX_PATH);
  PathCchRemoveFileSpec(appDirectory, MAX_PATH);

  // Create a ReactNativeWin32App with the ReactNativeAppBuilder
  auto reactNativeWin32App{winrt::Microsoft::ReactNative::ReactNativeAppBuilder().Build()};

  // Configure the initial InstanceSettings for the app's ReactNativeHost
  auto settings{reactNativeWin32App.ReactNativeHost().InstanceSettings()};
  wchar_t diagnosticPath[32768]{};
  const auto diagnosticLength = GetEnvironmentVariableW(L"ROCLICK_DIAGNOSTIC_LOG", diagnosticPath, 32768);
  if (diagnosticLength && diagnosticLength < 32768)
    settings.RedBoxHandler(winrt::make<ValidationErrors>(std::filesystem::path(diagnosticPath)));
  // Register any autolinked native modules
  RegisterAutolinkedNativeModulePackages(settings.PackageProviders());
  // Register any native modules defined within this app project
  settings.PackageProviders().Append(winrt::make<CompReactPackageProvider>());

#if BUNDLE
  // Load the JS bundle from a file (not Metro):
  // Set the path (on disk) where the .bundle file is located
  settings.BundleRootPath(std::wstring(L"file://").append(appDirectory).append(L"\\Bundle\\").c_str());
  // Set the name of the bundle file (without the .bundle extension)
  settings.JavaScriptBundleFile(L"index.windows");
  // Disable hot reload
  settings.UseFastRefresh(false);
#else
  // Load the JS bundle from Metro
  settings.JavaScriptBundleFile(L"index");
  // Enable hot reload
  settings.UseFastRefresh(true);
#endif
#if _DEBUG
  // For Debug builds
  // Enable Direct Debugging of JS
  settings.UseDirectDebugger(true);
  // Enable the Developer Menu
  settings.UseDeveloperSupport(true);
#else
  // For Release builds:
  // Disable Direct Debugging of JS
  settings.UseDirectDebugger(false);
  // Disable the Developer Menu
  settings.UseDeveloperSupport(false);
#endif

  // Get the AppWindow so we can configure its initial title and size
  auto appWindow{reactNativeWin32App.AppWindow()};
  appWindow.Title(L"RoClick");
  auto hwnd = winrt::Microsoft::UI::GetWindowFromWindowId(appWindow.Id());
  const auto dpi = static_cast<int>(GetDpiForWindow(hwnd));
  MONITORINFO monitor{sizeof(monitor)};
  GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
  const auto workWidth = monitor.rcWork.right - monitor.rcWork.left;
  const auto workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
  const auto width = std::min(MulDiv(460, dpi, 96), static_cast<int>(workWidth - 32));
  const auto height = std::min(MulDiv(820, dpi, 96), static_cast<int>(workHeight - 32));
  appWindow.MoveAndResize({monitor.rcWork.left + (workWidth - width) / 2,
    monitor.rcWork.top + (workHeight - height) / 2, width, height});
  appWindow.SetIcon(std::wstring(appDirectory).append(L"\\RoClick.ico"));
  roclick::appearance::Install(hwnd);
  auto &engine = roclick::Runtime::Shared();
  struct EngineLifetime {
    roclick::Runtime &engine;
    ~EngineLifetime() { engine.Shutdown(); }
  } lifetime{engine};
#if BUNDLE && !defined(_DEBUG)
  roclick::UpdateService::Shared().Initialize();
#endif

  // Get the ReactViewOptions so we can set the initial RN component to load
  auto viewOptions{reactNativeWin32App.ReactViewOptions()};
  viewOptions.ComponentName(L"RoClick");

  // Start the app
  reactNativeWin32App.Start();
  roclick::Runtime::Shared().Shutdown();
  return 0;
}

_Use_decl_annotations_ int CALLBACK WinMain(HINSTANCE, HINSTANCE, PSTR, int) {
  try { return RunApp(); }
  catch (winrt::hresult_error const &error) {
    MessageBoxW(nullptr, error.message().c_str(), (roclick::language::Get() == "tr" ? L"RoClick başlatılamadı — uygulamayı yeniden yükleyin" : L"RoClick could not start — reinstall the complete app"), MB_OK | MB_ICONERROR);
  }
  catch (std::exception const &) {
    MessageBoxW(nullptr, (roclick::language::Get() == "tr" ? L"Tıklama motoru başlatılamadı. RoClick’i kapatıp uygulamayı yeniden yükleyin." : L"The native engine could not start. Close RoClick and reinstall the complete app."), L"RoClick", MB_OK | MB_ICONERROR);
  }
  return 1;
}
