# RoClick

A local Windows auto-clicker with a React Native Windows desktop UI and a C++ input engine. Targets **x64 Windows 10 22H2 and Windows 11**. No Python, telemetry, automatic updates, or development server is required by the installed app.

**This migration is a release candidate, not a validated 1,000 clicks/sec release.** See [validation status](docs/VALIDATION.md). Published Python releases remain available below.

## Use

1. Set an interval from **1–1,000 ms** (default **10 ms**, equivalent to 100 clicks/sec).
2. Select Left, Right, or Middle.
3. Choose **Capture window**, then focus the target within two seconds. Capture runs natively.
4. Position the pointer where clicks should land. Press **1** to start, **2** or **Escape** to stop, or **F6** to toggle.
5. With target focus lock enabled (the default), clicking pauses when the captured window loses focus and resumes when it regains focus. Closing it stops the engine and requires recapture.

The digit shortcuts pass through to the foreground app. Typing 1 in RoClick does not start clicking; 2, F6 and Escape remain global. Typing 2 into a field also stops clicking. The UI Start/Stop button is also available. Capture stops an active session. Settings apply on the native worker, releasing any current press before changing buttons or timing.

**Speed means complete down/up clicks successfully submitted by `SendInput`.** The measured rate is a rolling native measurement, updated at most four times per second. It does not measure whether another app received or processed input. Target applications may process fewer clicks. **1,000 clicks/sec is a measured performance target, not a real-time guarantee.**

Advanced settings include focus lock, System/Light/Dark appearance, and press duration. **0 ms** submits down and up together for maximum speed. Longer presses reduce available speed and must be strictly below the interval. Rejected input stops clicking and displays a recovery message. RoClick never elevates itself automatically.

## Build on Windows

Requirements: x64 Windows, PowerShell 7, Node **22.14.0**, Visual Studio **2022** with Desktop development with C++, MSVC v143, Windows SDK **10.0.26100.0**, CMake, and Inno Setup **6.4.3**. RN **0.80.2**, RN Windows **0.80.6**, React **19.1.0**, and the dependency graph are pinned in `package.json` and `package-lock.json`.

```powershell
pwsh ./build.ps1
```

Output: `dist/RoClick-Setup.exe`, `dist/SHA256SUMS.txt`, and the separate `dist/roclick_benchmark.exe` validation tool. The installer bundles JavaScript/assets, Windows App SDK, RN, Hermes, and app-local MSVC runtime DLLs. It installs per user without requesting administrator privileges. Upgrade/reinstall uses a stable Inno AppId. Earlier portable releases are not deleted.

The **Build Windows installer** GitHub Actions workflow builds and uploads artifacts. It has read-only repository permissions and **does not create tags or publish releases**. Download the `RoClick-Windows-x64` artifact from its run. The diagnostics artifact contains the MSBuild binary log.

For development:

```powershell
npm ci
npm start
# Separate terminal:
npm run windows
```

For portable validation on Linux/macOS/Windows with a C++17 compiler:

```sh
npm run typecheck
npm run test:native
```

The Windows build separately compiles/tests the Win32 runtime. Linux tests use virtual time and simulated input; they are not Windows performance measurements.

## Design and architecture

Compact, resizable single window with the original logo, rounded opaque controls, blue accents, system typography, keyboard focus visuals, accessible labels, system high-contrast colors, and a scrolling layout for small windows/display scaling. There are no decorative block images or motion effects. This is an **iOS-inspired Windows design**.

Native DWM Desktop Acrylic is requested on supported Windows 11 builds. Windows 10, unsupported builds, disabled transparency, high contrast, and remote sessions use a solid backdrop. See [architecture and API references](docs/ARCHITECTURE.md).

- `src/App.tsx`: React Native controls and presentation.
- `src/native/NativeClickEngine.ts`: typed TurboModule contract.
- `native/core/Engine.h`: deterministic scheduling/input state machine.
- `native/windows/Runtime.*`: dedicated worker, waitable timers, process/handle identity, global keyboard and destruction hooks.
- `windows/RoClick/`: RN Windows New Architecture desktop host and C++ TurboModule.
- `native/tests/`: virtual-time tests and independent-process local receiver benchmark.
- `installer/`, `scripts/`, `.github/workflows/`: offline installer, validation, and artifact preparation.

## Screenshots

Reviewed captures of the **actual installed React Native UI**: [Light](docs/screenshots/roclick-light.png) · [Dark](docs/screenshots/roclick-dark.png). These come from the passing [Windows installer run](https://github.com/0xMasharipov/RoClick/actions/runs/41142581287) on Windows Server 2022. The small runner viewport shows the upper controls; the window scrolls to reveal the remaining controls. See [validation status](docs/VALIDATION.md) for outstanding Windows 10/11 acceptance.

## Existing published releases

These releases are unchanged and use the previous Python implementation.

| Version | Windows EXE | SHA-256 |
| --- | --- | --- |
| v0.2.3 | [RoClick.exe](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.3/RoClick.exe) | [RoClick.sha256.txt](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.3/RoClick.sha256.txt) |
| v0.2.2 | [RoClick.exe](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.2/RoClick.exe) | [RoClick.sha256.txt](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.2/RoClick.sha256.txt) |
| v0.2.1 | [RoClick.exe](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.1/RoClick.exe) | [RoClick.sha256.txt](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.1/RoClick.sha256.txt) |
| v0.2.0 | [RoClick.exe](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.0/RoClick.exe) | [RoClick.sha256.txt](https://github.com/0xMasharipov/RoClick/releases/download/v0.2.0/RoClick.sha256.txt) |
| v0.1.0 | [RoClick.exe](https://github.com/0xMasharipov/RoClick/releases/download/v0.1.0/RoClick.exe) | [RoClick.sha256.txt](https://github.com/0xMasharipov/RoClick/releases/download/v0.1.0/RoClick.sha256.txt) |

## License

MIT — see [LICENSE](LICENSE). Use on applications and systems you are authorized to automate.
