# Release acceptance record

**Status: implementation candidate. Windows 10/11 reference-machine acceptance is pending. No measured 1,000 clicks/sec claim is approved.**

## Automated evidence

| Check | Evidence/status |
| --- | --- |
| Portable engine tests | Passed locally with GCC; simulated 60-second schedules, cancellation, configuration, partial failures, focus, destruction and capture |
| TypeScript | Passed locally |
| Release JS bundle | Passed locally with Metro (production Windows bundle and logo asset) |
| Windows native build and installer | GitHub Actions run pending |
| Installation / bundled launch / reinstall / uninstall | Automated smoke script prepared; runner evidence pending |
| Clean Windows 10 and 11 without developer tools | Pending; a hosted build runner does not prove this |
| Native light/dark screenshots | Captured by Windows smoke test after installed UI passes accessibility-tree checks |

## Performance protocol

Use an idle x64 physical reference machine on **Windows 10 22H2** and another on **Windows 11**. Record OS build, CPU, RAM, power mode, display scaling, refresh rate, background processes and Git SHA. Keep power mode and workload consistent. No debugger or Metro during Release measurements. Leave the desktop unlocked, position the pointer in the local receiver and keep it focused. The benchmark deliberately rejects non-interactive sessions.

```powershell
pwsh ./scripts/benchmark.ps1 -Seconds 60 -Output artifacts/win11-idle
pwsh ./scripts/benchmark.ps1 -Seconds 60 -LoadThreads 4 -Output artifacts/win11-load
```

Repeat on Windows 10. Use a documented number of load threads relative to logical cores (e.g. cores minus one); the output records it. The tool uses the production native runtime at 10, 100, 500 and 1,000 clicks/sec, 60 seconds each. It reports submission and independently received counts, elapsed time, mean/stddev/min/max submission gaps, skipped deadlines, input failures, process CPU percentage (one-core basis), timer mode and stop latency. Load-thread CPU is included in process CPU in the load scenario; compare idle CPU separately. A nonzero result flags a missed target. Save raw JSON and metadata; never replace failures with inferred results.

| OS / scenario | 10 cps | 100 cps | 500 cps | 1,000 cps | Stop <50 ms | CPU / variation |
| --- | --- | --- | --- | --- | --- | --- |
| Windows 10 idle | Pending | Pending | Pending | Pending | Pending | Pending |
| Windows 11 idle | Pending | Pending | Pending | Pending | Pending | Pending |
| Windows 10 CPU load | Pending | Pending | Pending | Pending | Pending | Pending |
| Windows 11 CPU load | Pending | Pending | Pending | Pending | Pending | Pending |
| Windows 10 UI interaction / JS busy | Pending | Pending | Pending | Pending | Pending | Pending |
| Windows 11 UI interaction / JS busy | Pending | Pending | Pending | Pending | Pending | Pending |

Idle acceptance: average **submitted** rate within 5% at each speed; no catch-up bursts or stuck buttons; stop below 50 ms. The local receiver has its own message pump and counts completed mouse messages, not input API returns. Receiver losses are reported separately and do not imply any third-party app's acceptance. Native benchmark timing is not evidence for UI interaction or blocked-JS behavior.

## Runtime and UI tests on each OS

1. Run all speeds while opening Advanced, dragging/resizing, changing themes and tabbing controls. Returning to RoClick pauses a locked target; use unlocked mode only for the controlled UI-load experiment. Record rate/CPU/gaps, including pause periods. Confirm no submissions after Stop until another Start.
2. In a Debug validation session, block the JS thread for several seconds using the debugger console while the target is focused. Exercise native 2/Escape and F6. Measure key-to-final-release latency externally (ETW/input trace or a dedicated harness); UI event arrival is not the stop timestamp. Repeat with a 999 ms press at a 1,000 ms interval. Confirm the button is released without waiting for JS.
3. Hold F6: only one toggle until key-up. Check 1/2 and numpad equivalents outside RoClick. In numeric fields, digits must type normally; Escape still stops.
4. During a long press, change button/interval, lose target focus, close target, stop, and close RoClick. Confirm the original held button releases. Pause/resume must not replay elapsed clicks. Open a same-title window and recreate a window in the same process: neither may inherit the destroyed target.
5. Use a higher-integrity test receiver/another input desktop to provoke rejected SendInput. Check native failure state, actionable error, no automatic UAC/elevation, and release retry. The portable tests inject zero, partial and release failures deterministically.
6. Keyboard-only: Tab/Shift+Tab, Space/Enter, slider arrows/Home/End, editable interval, segments, capture/cancel, advanced controls. Screen reader: labels, selected/expanded states and state/error announcements. Stats must not chatter every 250 ms.
7. Inspect System/Light/Dark, high contrast, reduced motion, transparency off, remote desktop, and unsupported backdrop fallback. Check 100/125/150/175/200% scaling, moving across different-DPI monitors and smallest window size. Confirm opaque controls and no clipped critical actions.

## Installer and screenshots

On clean x64 Win10/11 VMs without Node, Python, Visual Studio or preinstalled Windows App SDK: disconnect network, install, launch, close, reinstall and uninstall. Verify shortcuts, correct per-user files, no development server/runtime download, and no unexpected UAC. Upgrade from the previous Inno candidate with a lower version and verify replacement/uninstall. Old published releases are portable; verify they remain available and untouched, not silently migrated/deleted.

`scripts/smoke-install.ps1` checks installed UI controls through UI Automation (a process existing is insufficient), captures real screenshots, reinstalls and uninstalls. CI smoke runs on Windows Server and does not substitute for Win10/11 or a clean machine. Review captures before adding them to `docs/screenshots/`. A same-version reinstall is not a version-upgrade test. Do not publish until all acceptance rows above have recorded evidence.
