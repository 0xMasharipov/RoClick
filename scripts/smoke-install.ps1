# CI smoke check; a clean Win10/11 VM is still required for release acceptance.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class CaptureWindow {
  [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
'@
$root = Split-Path $PSScriptRoot
$destination = Join-Path $env:TEMP 'RoClick-install-smoke'
$artifacts = Join-Path $root 'artifacts'
$screenshots = Join-Path $artifacts 'screenshots'
New-Item $screenshots -ItemType Directory -Force | Out-Null
$installer = Join-Path $root 'dist/RoClick-Setup.exe'
function Install-App([string]$phase) {
  $p = Start-Process $installer -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/DIR=`"$destination`"", "/LOG=`"$artifacts/$phase.log`"") -PassThru -Wait
  if ($p.ExitCode -ne 0) { throw "Installer $phase failed: $($p.ExitCode)" }
}
function Find-Control([string]$name) {
  $condition = [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty, $name)
  return $script:element.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
}
function Click-Control([string]$name) {
  $control = Find-Control $name
  if (-not $control) { throw "Missing UI control: $name" }
  $pattern = $control.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
  $pattern.Invoke()
  Start-Sleep -Milliseconds 500
}
function Screenshot([string]$name) {
  $rect = [CaptureWindow+Rect]::new()
  [CaptureWindow]::GetWindowRect($script:app.MainWindowHandle,[ref]$rect) | Out-Null
  $bitmap = [Drawing.Bitmap]::new($rect.Right-$rect.Left,$rect.Bottom-$rect.Top)
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.CopyFromScreen($rect.Left,$rect.Top,0,0,$bitmap.Size)
    $bitmap.Save((Join-Path $screenshots "$name.png"),[Drawing.Imaging.ImageFormat]::Png)
  } finally { $graphics.Dispose(); $bitmap.Dispose() }
}
Install-App 'install'
$app = Start-Process (Join-Path $destination 'RoClick.exe') -WorkingDirectory $destination -PassThru
try {
  for ($i=0; $i -lt 90; $i++) {
    Start-Sleep -Milliseconds 500
    $app.Refresh()
    if ($app.HasExited) { throw "Installed app exited: $($app.ExitCode)" }
    if ($app.MainWindowHandle -ne 0) {
      $element = [System.Windows.Automation.AutomationElement]::FromHandle($app.MainWindowHandle)
      if (Find-Control 'Capture window') { break }
    }
  }
  if (-not (Find-Control 'Capture window')) {
    if ($app.MainWindowHandle -ne 0) { Screenshot 'startup-failure' }
    throw 'React UI did not load from bundled JavaScript.'
  }
  [CaptureWindow]::SetForegroundWindow($app.MainWindowHandle) | Out-Null
  Click-Control 'Advanced settings +'
  Click-Control 'Light'
  Click-Control 'Advanced settings −'
  Screenshot 'roclick-light'
  Click-Control 'Advanced settings +'
  Click-Control 'Dark'
  Click-Control 'Advanced settings −'
  Screenshot 'roclick-dark'
  $app.CloseMainWindow() | Out-Null
  if (-not $app.WaitForExit(10000)) { throw 'App did not close cleanly.' }
  Install-App 'reinstall'
  $uninstall = Start-Process (Join-Path $destination 'unins000.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$artifacts/uninstall.log`"") -Wait -PassThru
  if ($uninstall.ExitCode -ne 0 -or (Test-Path (Join-Path $destination 'RoClick.exe'))) { throw 'Uninstall failed.' }
  @{install='passed'; bundledLaunch='passed'; reinstall='passed'; uninstall='passed'; cleanMachine='not tested'; versionUpgrade='not tested'; os=[Environment]::OSVersion.VersionString} | ConvertTo-Json | Set-Content (Join-Path $artifacts 'smoke-install.json')
} finally {
  if (-not $app.HasExited) { Stop-Process -Id $app.Id -ErrorAction SilentlyContinue }
}
