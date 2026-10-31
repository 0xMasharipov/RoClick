#pragma once
#include "NativeModules.h"
#include "AppLanguage.h"
#include "../../native/windows/UpdateService.h"

namespace RoClick {
REACT_STRUCT(UpdateState)
struct UpdateState {
  REACT_FIELD(enabled) bool enabled = true;
  REACT_FIELD(state) std::string state;
  REACT_FIELD(version) std::string version;
  REACT_FIELD(availableVersion) std::string availableVersion;
};
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
  REACT_METHOD(getUpdateState)
  void getUpdateState(winrt::Microsoft::ReactNative::ReactPromise<UpdateState> const &promise) noexcept {
    try {
      const auto state = roclick::UpdateService::Shared().Read();
      promise.Resolve({state.enabled, state.state, state.version, state.availableVersion});
    } catch (...) { promise.Reject("Could not read update status."); }
  }
  REACT_METHOD(setAutomaticUpdates)
  void setAutomaticUpdates(bool enabled, winrt::Microsoft::ReactNative::ReactPromise<void> const &promise) noexcept {
    if (roclick::UpdateService::Shared().SetEnabled(enabled)) promise.Resolve();
    else promise.Reject("Could not save automatic updates preference.");
  }
};
}
