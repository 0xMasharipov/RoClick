$ErrorActionPreference = "Stop"
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
pip install pyinstaller==6.10.0
pyinstaller --noconfirm --clean --windowed --onefile `
  --name RoClick `
  --icon assets\roclick.ico `
  --paths . `
  run.py
Write-Host "Built executable: dist\RoClick.exe"
