# RoClick

RoClick is a lightweight Windows desktop auto-clicker with a clean, iOS-inspired interface. It repeatedly sends ordinary mouse clicks at a user-selected interval and can restrict clicking to one captured foreground window.

## What it is for

RoClick is intended for accessibility workflows, repetitive desktop tasks, interface testing, personal automation, and private QA environments where automated clicking is permitted. It is not designed to bypass anti-cheat systems, automate competitive online gameplay, read process memory, inject code, or avoid platform restrictions.

## Features

- Minimal iOS-style desktop interface
- Global **F6** start/stop hotkey
- **Escape** emergency stop
- 50–1000 ms click interval
- Left, right, and middle click modes
- Foreground-window capture
- Optional target-window focus lock
- Local-only operation with no telemetry
- Reproducible Windows EXE build through GitHub Actions

## Logo

The logo combines a rounded blue app tile, a bold letter **R**, and a small circular click indicator. The rounded geometry references modern mobile interface design, while the blue gradient communicates clarity, control, and utility.

## Run from source

```bash
git clone https://github.com/YOUR_USERNAME/roclick.git
cd roclick
python -m venv .venv
```

Windows PowerShell:

```powershell
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python run.py
```

## Build the Windows executable

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\build.ps1
```

Output:

```text
dist\RoClick.exe
```

Alternatively, open the repository's **Actions** tab, run **Build Windows EXE**, and download the `RoClick-Windows` artifact.

## Usage

1. Open RoClick.
2. Choose a click interval and mouse button.
3. Focus the application you are allowed to automate.
4. Return to RoClick and select **Capture Active** after focusing the intended target as directed.
5. Keep **Click only while target is active** enabled for safer operation.
6. Press **F6** to start or stop.
7. Press **Escape** at any time for an emergency stop.

## Project structure

```text
RoClick/
├── .github/workflows/build-windows.yml
├── assets/roclick-logo.svg
├── docs/
├── src/roclick/
│   ├── __init__.py
│   ├── app.py
│   └── window_utils.py
├── AGENT_SPEC.md
├── build.ps1
├── README.md
├── requirements.txt
└── run.py
```

## Responsible use

Use RoClick only on systems and applications you own or are authorized to automate. Some games and online services prohibit automation even when it uses ordinary mouse input. The user is responsible for checking and following the applicable rules.

## GitHub publishing

```bash
git init
git add .
git commit -m "Initial RoClick release"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/roclick.git
git push -u origin main
```

## License

Choose a license before public release. MIT is suitable for a permissive open-source project, but review whether it fits your goals.
