# Native desktop architecture

The installed app is an unpackaged x64 Win32 process hosting RN Windows Fabric/Hermes. React renders controls only. The process starts `Runtime` before loading JavaScript and stops it before/at native window shutdown. A busy JS thread cannot own scheduling, capture deadlines, key handling, or release work.

## Scheduling and submission

One worker owns `Engine`, its configuration and the target process handle. Commands cross a bounded/coalescing queue. Stop clears queued actions (preserving the latest configuration) and signals the worker's wait event. Keyboard hooks run on a separate native message thread; repeated key-downs are ignored. Keys are never swallowed or logged. No hook or scheduler callback invokes JavaScript.

Time is `QueryPerformanceCounter`, converted to monotonic nanoseconds. Each interval has an absolute monotonic deadline. A one-shot high-resolution waitable timer supplies an interruptible relative wait; a separate event cancels that wait. A standard waitable timer is used if the high-resolution creation fails, and the UI reports that fallback. Windows scheduling is not real time.

A late wake submits at most one click. Expired slots are counted and discarded, never replayed in batches. Small wake jitter retains the deadline phase; a following gap that would be below 90% of the interval resets to one full interval. Long presses have a separate release deadline. All waits are interruptible and active target checks occur no farther apart than 10 ms, including during a press. Stopped/error states use a one-second heartbeat; commands and window destruction wake the worker immediately. Delayed capture sleeps until its deadline. The command deque is reused instead of allocated on each tick.

At 0 ms, a single `SendInput` call contains exactly one down/up pair. A count of 2 increments submitted clicks; 1 means down succeeded and triggers a release attempt; 0 increments no clicks. Timed presses count only after both their down and scheduled up have succeeded. Cancellation releases are included in input-event totals but not completed clicks. A failed release retains the held-button identity so Stop and shutdown can retry. Windows can reject even a cleanup release; the app reports this explicitly instead of claiming it can guarantee physical button state.

The rolling submission rate uses elapsed monotonic wall time, including pauses. Per-session counters reset on Start. Gap summaries exclude pause/configuration discontinuities. The worker publishes statistics at most every 250 ms, with immediate publication for commands, state transitions and target changes. Unchanged idle snapshots and appearance do not produce new JS events. Statistics run on a separate 250 ms reporter, with one outstanding event awaiting JS acknowledgment; blocked JS cannot create an unbounded event queue or stall the worker. UI state is native-authoritative and may be displayed up to 250 ms later than the actual stop.

## Target lifetime

Native discovery checks the foreground window first, then enumerates visible, unowned top-level windows. It matches the process executable basename against `RobloxPlayerBeta.exe`, `RobloxPlayer.exe` and `Windows10Universal.exe`, ignoring case; titles are display-only. Discovery runs at startup and once per second while disconnected, and the Find Roblox action searches immediately. Clear target suspends automatic discovery until Find Roblox or Start. The benchmark explicitly uses foreground capture mode to target its independent input receiver.

Capture records the root HWND, PID, window thread, retained process handle and a creation timestamp checked at capture. The retained handle pins the original process identity; its creation timestamp is not queried again per click. Foreground root identity is checked before a down. Process exit, HWND destruction or identity mismatch invalidates the target. A native `EVENT_OBJECT_DESTROY` hook latches destruction, including a destroyed handle that is later reused. Closing the target stops clicking even if focus lock was disabled. Idle discovery can reconnect a new player window, but leaves clicking stopped until the user starts again. Discovery does not clear input/release, configuration, monitoring or scheduler errors. Window/process checks and submission cannot be one atomic Windows operation; the final check narrows but cannot eliminate a focus change race.

SendInput operates at the pointer position. This is not background-window injection and does not move the pointer. Keep the pointer over the intended receiver. UIPI/input-desktop restrictions can reject input; Windows does not reliably identify UIPI as the cause. The error offers permission/desktop checks without automatic elevation or claiming a definitive cause from `GetLastError`.

## UI and packaging

The refreshed RoClick logo is used in the header, executable, title bar and installer. Opaque panels keep text readable. Documented DWM `DWMSBT_TRANSIENTWINDOW` requests Desktop Acrylic on Windows 11 build 22621+; unsupported OS/API results, disabled transparency, high contrast and remote sessions select solid fills. Appearance changes are handled natively. React uses system high-contrast colors, labels, focus indicators, scalable layout and no animation (therefore no reduced-motion exception).

The installer contains the generated JS bundle/assets, self-contained Windows App SDK files, RN/Hermes libraries and app-local redistributable MSVC CRT files. The Windows 10 OS supplies UCRT. Runtime dependencies are bundled for offline installation. The app and installer run per user and do not change process integrity. There is no telemetry. Developer Metro support is disabled for Release.

## Automatic updates

Installed release builds start a hidden Windows PowerShell helper using the system executable and a copied, bundled `update.ps1`. `version.txt` is generated from the same package version passed to Inno Setup. All network requests, hashing and installation run outside the input worker and JS thread. No service, scheduled task, credentials or machine-wide policy changes are used. Debug/Metro builds do not launch the helper.

The helper checks the anonymous GitHub latest-release endpoint on launch and every six hours. Only newer stable numeric x.y.z versions are accepted. Installer and checksum assets must be unique, bounded in size and point to the matching release in this repository over HTTPS. Downloads have timeouts and byte limits. The published SHA-256 checksum and installer file-version resource are checked before execution; the installed version is checked again to prevent downgrading a newer manual install. Offline/network errors retry after 15 minutes without blocking the app.

Each install directory has a shared update lock and each helper has a separate job directory under `%LOCALAPPDATA%\RoClick\Updates`. The helper validates the original app's PID, process creation timestamp and executable path, then retains its process object while waiting for exit. A pending update waits for all matching app processes to close. Native startup and installation share a mutex gate so a new launch cannot open partially replaced files. An application mutex exists from native startup, and Inno Setup's AppMutex plus `/NOCLOSEAPPLICATIONS` prevents open windows from being forcibly closed. The silent installer uses the existing installation directory, suppresses restarts and verifies the new version file afterward. It does not reopen the app after the user closes it.

`HKCU\Software\RoClick\AutomaticUpdates` defaults to enabled. Disabling it prevents network checks and cancels pending installation. The helper checks this preference while waiting and before installation. It exits when the app closes and removes its job directory. Installation errors are retained in `last-error.txt` under that installation's update-cache directory; a subsequent launch checks again. The AppSettings TurboModule exposes the preference, running version and update phase to localized UI controls; downloaded/ready notices are visible without opening Advanced. A verified installer version is required before the helper advertises ready. Windows share the owning helper's status through a generated active-job identifier.

A ready update immediately opens a localized Install now / Later modal once per version per session. Native attention restores a minimized window and requests foreground activation, falling back to taskbar flashing when Windows blocks it. Later keeps deferred installation; the notice can reopen the modal. Install now rechecks ready/enabled state, then posts a registered shutdown message only to marked RoClick windows whose executable paths match the current installation. Each worker rejects new start commands, stops and publishes its release result before its window closes. A failed release prevents that window from closing for the update. No process is forcibly terminated. The helper remains responsible for waiting for all processes and verifying the installer again.

Offline PowerShell tests cover stable version ordering, trusted release paths, bounded assets, checksum ambiguity and tampering, installer version, downgrade prevention, install-result checks, waiting for all windows to close, disabling checks and cancelling a staged update. Installed-app CI smoke checks launch the bundled helper with updates disabled so the test does not depend on live releases. A real two-version Windows upgrade still requires acceptance validation.

## Localization and preferences

`AppSettings` is a separate TurboModule from `ClickEngine`. `getLanguage()` resolves `en` or `tr` from a valid `HKCU\Software\RoClick\Language` override, otherwise from the first Windows preferred UI language (Turkish or English fallback). `setLanguage()` saves the explicit choice for the current user. No preferences I/O runs on the scheduler worker. Native startup dialogs use the same language resolver.

React waits for the initial language before rendering controls. Runtime switches update the existing component tree, preserving configuration, input drafts, target and engine state. Save failures leave the session language usable and show a translated warning. English and Turkish dictionaries have matching typed keys and placeholders. Numeric formatting is explicit because this Windows Hermes configuration does not reliably support locale-aware formatting.

Native `Snapshot.errorKind` maps to a stable `EngineEvent.errorKey`; Windows `errorCode` and English diagnostic text remain available. The React UI translates error keys, including missing targets, capture failures, rejected input/releases, and monitoring/timer/wait failures. Presentation language never enters the scheduler state machine.

The generated character artwork is bundled locally. App-header text is rendered by React so it translates; high contrast hides the decorative image. Documentation showcase graphics lay out actual installed-app PNGs without redrawing their contents.

## Primary references

- [RN Windows desktop templates](https://microsoft.github.io/react-native-windows/docs/0.80/init-windows-cli/)
- [High-resolution waitable timer creation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)
- [SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex)
- [SendInput results and UIPI limitations](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)
- [DWM system backdrop types and supported Windows version](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type)
- [Self-contained Windows App SDK deployment](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/self-contained-deploy/deploy-self-contained-apps)
