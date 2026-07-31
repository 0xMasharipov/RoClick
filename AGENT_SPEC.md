# RoClick — AI Coding Agent Specification

## Mission
Build **RoClick**, a polished Windows desktop auto-clicker with a clean iOS-inspired interface. It is a general productivity, accessibility, QA, and private-testing utility. It must not include process injection, memory reading, anti-cheat bypasses, hidden automation, CAPTCHA circumvention, or game-specific exploit logic.

## Product description
RoClick repeatedly sends ordinary mouse clicks at a user-selected interval. The user explicitly starts and stops the action. A target-window lock prevents clicks from being sent when the selected application is not focused.

## Core requirements
1. Python 3.12+ desktop app.
2. CustomTkinter interface with rounded white cards, light-gray background, blue primary action, Apple-like spacing, and readable typography.
3. Start/stop button and global F6 hotkey.
4. Emergency stop with Escape.
5. Adjustable interval from 50–1000 ms.
6. Left, right, and middle mouse-button selection.
7. Capture the current foreground-window title.
8. Optional lock that permits clicks only while the captured window is active.
9. Always-visible status: Stopped or Active.
10. Graceful shutdown of keyboard listener and worker thread.
11. No admin rights required.
12. No network access, telemetry, accounts, ads, or background updater.

## Architecture
- `run.py`: application entry point.
- `src/roclick/app.py`: UI, state, keyboard listener, click loop.
- `src/roclick/window_utils.py`: foreground-window abstraction.
- `assets/`: logo and icon files.
- `.github/workflows/build-windows.yml`: reproducible Windows build.

## Safety boundaries
The coding agent must reject features that:
- identify or target a particular online game for unattended farming;
- inspect another process’s memory;
- inject DLLs or scripts;
- evade anti-cheat or platform restrictions;
- conceal automation from the user;
- bypass rate limits or human-verification mechanisms.

Permitted features include ordinary OS-level input for applications where the user has permission, explicit hotkeys, visible state, accessibility controls, and local QA testing.

## UI specification
- Window: approximately 460×620 px.
- Background: `#F2F2F7`.
- Cards: white, 22 px corner radius.
- Primary blue: `#0A84FF`.
- Destructive stop red: `#FF453A`.
- Active green: `#30D158`.
- Primary text: `#111111`.
- Secondary text: `#6E6E73`.
- Header contains a rounded blue logo tile and RoClick title.
- Status card contains state indicator, state label, helper text, and primary button.
- Settings card contains interval slider, button selector, target capture, lock switch, and safety hint.

## Functional acceptance tests
1. Launching `python run.py` opens one window without a console error.
2. Pressing F6 changes the status to Active.
3. Pressing F6 again changes status to Stopped.
4. Pressing Escape always stops clicking.
5. At 100 ms, approximately ten clicks are emitted per second.
6. Selecting Right emits right-clicks.
7. With target lock enabled and no target selected, no clicks are emitted.
8. With a target selected, clicks pause immediately when focus leaves it.
9. Closing the window terminates the listener and worker cleanly.
10. The Windows workflow creates `RoClick.exe` as a downloadable artifact.

## Build instructions
Use the provided PowerShell script on Windows:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\build.ps1
```

The finished executable should appear at `dist\RoClick.exe`.

## Completion checklist
- [ ] All acceptance tests pass.
- [ ] README screenshots or mockups added.
- [ ] SVG converted to ICO at 16, 32, 48, 64, 128, and 256 px.
- [ ] Windows executable built.
- [ ] SHA-256 checksum published with each release.
- [ ] No antivirus-evasion flags or packers used.
- [ ] Repository license selected and included.
