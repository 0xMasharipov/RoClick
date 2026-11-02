# Release acceptance record

**Status: v1.1.2 installer published for download. Windows 10/11 gameplay and reference-machine acceptance remain pending. No 1,000 clicks/sec guarantee is approved.**

## v1.1.2 release evidence

[Windows run 41522411371](https://github.com/0xMasharipov/RoClick/actions/runs/41522411371), commit `7032449`, passed portable and Windows native tests, TypeScript/localization, MSVC app/installer compilation, installed defaults/presets/mode switching, fractional-rate editing, automatic upgrades and native performance comparison. The public stable [v1.1.2 release](https://github.com/0xMasharipov/RoClick/releases/tag/v1.1.2) attaches the installer, checksums, benchmark, installed smoke/upgrade results and raw native measurements. Downloaded build hashes and GitHub asset digests agree.

The direct 1 ms request with game-friendly mode enabled passed all three native trials (20.346, 19.898 and 19.903 clicks/sec). These roughly two-second measurements include the immediate first click; the finite-window acceptance bound is 20.5 clicks/sec. Portable tests separately verify at least 50 ms between down submissions. Unrestricted throughput trials reported zero input failures and maximum observed stop latency of 0.359 ms. Their median 500/1,000 configured rates were approximately 440/792 clicks/sec on the hosted runner, below the performance targets; overall CPU and gameplay improvements are not established.

v1.1.2 defaults to game-friendly mode at 50 ms / 20 clicks/sec, with a 100 ms / 10 clicks/sec preset and explicit unrestricted mode. Portable tests verify the native cap for 1/2/10/50 ms requests, slower settings, a 60-second schedule, jitter and stalls without compressed intervals, focus pause/resume, mode changes during held presses, and unrestricted throughput. TypeScript and English/Turkish localization checks pass locally.

The Windows benchmark additionally tests a direct native 1 ms request with game-friendly mode enabled, requiring output at or below 20.5 clicks/sec (finite measurement window), no failures or held button, and stop below 50 ms. Its four legacy throughput trials explicitly disable game-friendly mode; the baseline harness has no mode field. CI also checks installed defaults/presets, localization and automatic upgrades.

Installed testing exposed two UI issues: switching from 1 ms back to game-friendly mode could leave the native interval editor stale, and a fractional rate such as 75 ms (13.33 clicks/sec) caused RNW's integer accessibility conversion to clear the release UI. The editor resets when its allowed range changes; slider accessibility uses a rounded integer with the exact rate retained in its text. Smoke checks cover both transitions and require the UI to remain present after editing. An opt-in `ROCLICK_DIAGNOSTIC_LOG` path captures local UI errors during CI; normal launches write no diagnostic log.

**Gameplay evidence is pending.** A user reported Blade Ball on a Türkiye server increasing from 40–50 ms to 60–100 ms at the 1 ms setting. Reducing click load is a mitigation to evaluate, not a verified server-ping fix. Compare stopped/20/10 clicks/sec in the same server while recording network ping, data ping and FPS. Do not infer game latency from native benchmark results.

## v1.1.1 release evidence

[Windows run 41365958537](https://github.com/0xMasharipov/RoClick/actions/runs/41365958537), commit `015a4de`, passed portable tests, Windows PowerShell tests, native discovery/release tests, MSVC app/installer compilation, installed-app smoke, automatic upgrades and performance comparison. Installer and benchmark download hashes match the build's `SHA256SUMS.txt`.

Installed upgrade tests use actual v1.1.0, v1.1.1 and an **unpublished v1.1.2 fixture**, replacing only the download boundary with local assets. They verify v1.1.0 → v1.1.1 installation after normal exit, the immediate English/Turkish popup, Later without repeated prompting, disabling a pending update, preference persistence, and Install now closing two matching app windows before installation. The legacy test uses normal Windows PowerShell module paths; the candidate also handles inherited PowerShell 7 paths. Actual popup captures are in the run's artifact (`update-popup-en.png`, `update-popup-tr.png`); their 1.1.2 labels belong to the private fixture.

The native comparison runs three alternating trials per version on the same Windows Server 2022 hosted runner, 10 seconds per rate. Both use the same independent receiver/harness; the baseline worker implementation is unchanged except for the identical read-only cycle accessor.

| Configured clicks/sec | Baseline worker cycles/click | v1.1.1 worker cycles/click | Cycle reduction | v1.1.1 submitted clicks/sec |
| --- | --- | --- | --- | --- |
| 10 | 1,402,110 | 921,682 | 34.3% | 10.10 |
| 100 | 258,595 | 188,143 | 27.2% | 100.04 |
| 500 | 168,881 | 149,817 | 11.3% | 432.23 |
| 1,000 | 148,098 | 132,997 | 10.2% | 784.13 |

No input failures were observed; maximum observed stop latency was 0.397 ms. **Whole-process CPU readings were mixed:** the median one-core percentages at 500 and 1,000 configured clicks/sec rose from 1.09% to 1.25% and 2.50% to 2.65%, while the 100 clicks/sec sample fell from 1.87% to 0.47%. These accounting samples are coarse; no overall CPU reduction or Roblox frame-rate improvement is established. Worker cycles measure scheduling work and cannot be converted to elapsed time across CPU frequencies. Neither version achieved the fastest rate targets on this runner.

The release attaches [performance-comparison.json](https://github.com/0xMasharipov/RoClick/releases/download/v1.1.1/performance-comparison.json), [raw performance trials](https://github.com/0xMasharipov/RoClick/releases/download/v1.1.1/native-performance-evidence.zip), and [installed-upgrade results](https://github.com/0xMasharipov/RoClick/releases/download/v1.1.1/upgrade-tests.json). Real Roblox gameplay measurements and clean Windows 10/11 upgrades remain outstanding.

## Earlier v1.1.0 automated evidence

| Check | Evidence/status |
| --- | --- |
| Portable engine tests | Passed locally with GCC; simulated 60-second schedules, cancellation, configuration, partial failures, focus, destruction and capture |
| TypeScript | Passed locally |
| English/Turkish dictionaries and formatting | Passed locally: matching keys/placeholders, all native error identifiers, Turkish glyphs, grouped and decimal numbers |
| Automatic-update offline tests | Passed locally with PowerShell 7: stable versions, asset origin/limits, checksum tampering, downgrade prevention, deferred installation, disabling checks, cancelling pending updates, installer flags/results and reopened-window checks; Windows build also runs the tests with Windows PowerShell 5.1 |
| Automatic-update native helper | Passed MSVC build and installed-helper launch/disabled-preference smoke on Windows Server 2022 in [run 41233456606](https://github.com/0xMasharipov/RoClick/actions/runs/41233456606) |
| Automatic Roblox discovery | Passed Windows native discovery tests and installed-app smoke with an independent player fixture; unrelated windows were rejected |
| Real Windows automatic upgrade between two releases | Pending; requires two updater-enabled versioned installers and verification on Windows 10/11 |
| Release JS bundle | Passed locally with Metro (production Windows bundle and logo asset) |
| Windows native build and installer | Passed on Windows Server 2022 in [run 41233456606](https://github.com/0xMasharipov/RoClick/actions/runs/41233456606), release commit `df440cb`; installer published as [v1.1.0](https://github.com/0xMasharipov/RoClick/releases/tag/v1.1.0) |
| Installation / bundled launch / reinstall / uninstall | Passed in the same run; downloaded `smoke-install.json` confirms all four checks |
| Clean Windows 10 and 11 without developer tools | Pending; a hosted build runner does not prove this |
| English/Turkish native light/dark screenshots | New v1.1.0 captures are in the release build's artifact. The reviewed documentation screenshots in [screenshots/](screenshots/) were captured in [v1.0.0 run 41149902615](https://github.com/0xMasharipov/RoClick/actions/runs/41149902615); narrow Turkish and expanded Advanced captures are included |
| Language persistence and live switching | Passed: Turkish survives app restart; switching keeps interval/button settings and a paused native session |
| Native errors and input validation | Passed: native capture errors and invalid numeric-input messages translate immediately; unedited fields do not produce spurious validation |
| Showcase sheet | PNG and editable SVG assembled from the actual four installed-app screenshots |
| High contrast, physical hotkeys, multi-DPI and clean Win10/11 review | Reference-machine acceptance remains pending |

Downloaded v1.1.0 installer and benchmark SHA-256 checksums both match `dist/SHA256SUMS.txt` from this run and GitHub's uploaded-asset digests. The installer resources identify file version `1.1.0.0` and product version `1.1.0`. The four main screenshots show the top of the scrollable window at the runner's available desktop size. The narrow capture uses a 360 px outer window; the Advanced capture intentionally scrolls to the expanded controls. CI uses injected keyboard input for numeric editing; native hotkeys intentionally ignore injected keys, so this does not prove physical hotkey acceptance.

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
3. Hold F6: only one toggle until key-up. Check 1/2 and numpad equivalents outside RoClick. In numeric fields, digits must type normally; typing 1 must not start; 2/Escape must still stop.
4. During a long press, change button/interval, lose target focus, close target, stop, and close RoClick. Confirm the original held button releases. Pause/resume must not replay elapsed clicks. Open a same-title window and recreate a window in the same process: neither may inherit the destroyed target.
5. Use a higher-integrity test receiver/another input desktop to provoke rejected SendInput. Check native failure state, actionable error, no automatic UAC/elevation, and release retry. The portable tests inject zero, partial and release failures deterministically.
6. Keyboard-only: Tab/Shift+Tab, Space/Enter, slider arrows/Home/End, editable interval, segments, capture/cancel, advanced controls. Screen reader: labels, selected/expanded states and state/error announcements. Stats must not chatter every 250 ms.
7. Inspect System/Light/Dark, high contrast, reduced motion, transparency off, remote desktop, and unsupported backdrop fallback. Check 100/125/150/175/200% scaling, moving across different-DPI monitors and smallest window size. Confirm opaque controls and no clipped critical actions.

## Installer and screenshots

On clean x64 Win10/11 VMs without Node, Python, Visual Studio or preinstalled Windows App SDK: disconnect network, install, launch, close, reinstall and uninstall. Verify shortcuts, correct per-user files, no development server/runtime download, and no unexpected UAC. Upgrade from the previous Inno candidate with a lower version and verify replacement/uninstall. Old published releases are portable; verify they remain available and untouched, not silently migrated/deleted.

`scripts/smoke-install.ps1` checks installed UI controls through UI Automation (a process existing is insufficient), captures real screenshots, reinstalls and uninstalls. CI smoke runs on Windows Server and does not substitute for Win10/11 or a clean machine. Review captures before adding them to `docs/screenshots/`. A same-version reinstall is not a version-upgrade test. The published installer does not imply reference-machine acceptance; keep these pending checks explicit in release notes until all rows above have recorded evidence.

For automatic-update acceptance, install an older updater-enabled build and publish a newer stable test release with its matching installer/checksums. Confirm detection/download, no interruption during active clicking, waiting for all app windows to close, silent installation into the same directory, and the new version on next launch. Repeat with updates disabled before checking and after download, offline access, a bad checksum, installer failure, a manually installed newer version, and an app launch racing installation. Verify preference persistence and English/Turkish status messages. Keep regular installer smoke tests offline with `AutomaticUpdates=0`.
