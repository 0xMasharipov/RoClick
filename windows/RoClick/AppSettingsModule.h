#pragma once
#include "NativeModules.h"
#include "AppLanguage.h"

namespace RoClick {
REACT_TURBO_MODULE(AppSettingsModule, L"AppSettings")
struct AppSettingsModule {
  REACT_METHOD(getLanguage)
  void getLanguage(winrt::Microsoft::ReactNative::ReactPromise<std::string> const &promise) noexcept {
    promise.Resolve(roclick::language::Get());
  }
  REACT_METHOD(setLanguage)
  void setLanguage(std::string value, winrt::Microsoft::ReactNative::ReactPromise<void> const &promise) noexcept {
    if (roclick::language::Set(value)) promise.Resolve();
    else promise.Reject("Could not save the language preference.");
  }
};
}
