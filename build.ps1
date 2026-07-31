$ErrorActionPreference = "Stop"
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
pip install pyinstaller==6.21.0
python -m PyInstaller --noconfirm --clean --windowed --onefile `
  --name RoClick `
  --icon assets\roclick.ico `
  --add-data "assets;assets" `
  --collect-data customtkinter `
  --paths . `
  run.py
Write-Host "Built executable: dist\RoClick.exe"
