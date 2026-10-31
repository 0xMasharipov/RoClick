# Native desktop architecture

The installed app is an unpackaged x64 Win32 process hosting RN Windows Fabric/Hermes. React renders controls only. The process starts `Runtime` before loading JavaScript and stops it before/at native window shutdown. A busy JS thread cannot own scheduling, capture deadlines, key handling, or release work.

## Scheduling and submission

One worker owns `Engine`, its configuration and the target process handle. Commands cross a bounded/coalescing queue. Stop clears queued actions (preserving the latest configuration) and signals the worker's wait event. Keyboard hooks run on a separate native message thread; repeated key-downs are ignored. Keys are never swallowed or logged. No hook or scheduler callback invokes JavaScript.

Time is `QueryPerformanceCounter`, converted to monotonic nanoseconds. Each interval has an absolute monotonic deadline. A one-shot high-resolution waitable timer supplies an interruptible relative wait; a separate event cancels that wait. A standard waitable timer is used if the high-resolution creation fails, and the UI reports that fallback. Windows scheduling is not real time.

A late wake submits at most one click. Expired slots are counted and discarded, never replayed in batches. Small wake jitter retains the deadline phase; a following gap that would be below 90% of the interval resets to one full interval. Long presses have a separate release deadline. All waits are interruptible and target checks occur no farther apart than 10 ms, including during a press.

At 0 ms, a single `SendInput` call contains exactly one down/up pair. A count of 2 increments submitted clicks; 1 means down succeeded and triggers a release attempt; 0 increments no clicks. Timed presses count only after both their down and scheduled up have succeeded. Cancellation releases are included in input-event totals but not completed clicks. A failed release retains the held-button identity so Stop and shutdown can retry. Windows can reject even a cleanup release; the app reports this explicitly instead of claiming it can guarantee physical button state.

The rolling submission rate uses elapsed monotonic wall time, including pauses. Per-session counters reset on Start. Gap summaries exclude pause/configuration discontinuities. Statistics run on a separate 250 ms reporter, with one outstanding event awaiting JS acknowledgment; blocked JS cannot create an unbounded event queue or stall the worker. UI state is native-authoritative and may be displayed up to 250 ms later than the actual stop.

## Target lifetime

Delayed capture records the root HWND, PID, window thread, retained process handle and creation timestamp. Titles are display-only. Foreground root identity is checked before a down. Process exit, HWND destruction or identity mismatch invalidates the target. A native `EVENT_OBJECT_DESTROY` hook latches destruction, including a destroyed handle that is later reused. Closing the target requires recapture even if focus lock was disabled. Window/process checks and submission cannot be one atomic Windows operation; the final check narrows but cannot eliminate a focus change race.

SendInput operates at the pointer position. This is not background-window injection and does not move the pointer. Keep the pointer over the intended receiver. UIPI/input-desktop restrictions can reject input; Windows does not reliably identify UIPI as the cause. The error offers permission/desktop checks without automatic elevation or claiming a definitive cause from `GetLastError`.

## UI and packaging

The refreshed RoClick logo is used in the header, executable, title bar and installer. Opaque panels keep text readable. Documented DWM `DWMSBT_TRANSIENTWINDOW` requests Desktop Acrylic on Windows 11 build 22621+; unsupported OS/API results, disabled transparency, high contrast and remote sessions select solid fills. Appearance changes are handled natively. React uses system high-contrast colors, labels, focus indicators, scalable layout and no animation (therefore no reduced-motion exception).

The installer contains the generated JS bundle/assets, self-contained Windows App SDK files, RN/Hermes libraries and app-local redistributable MSVC CRT files. The Windows 10 OS supplies UCRT. Nothing downloads at app launch/install. The app and installer run per user and do not change process integrity. There are no telemetry/update/network features in the product. Developer Metro support is disabled for Release.

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
