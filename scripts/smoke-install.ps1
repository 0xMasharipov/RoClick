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
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int width, int height, bool repaint);
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
function Find-Control([string]$id) {
  $condition = [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty, $id)
  if (-not $script:element) { return $null }
  return $script:element.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
}
function Click-Control([string]$name) {
  $control = Find-Control $name
  if (-not $control) { throw "Missing UI control: $name" }
  if (-not $control.Current.IsEnabled) { throw "UI control is disabled: $name" }
  if ($control.Current.IsOffscreen) {
    $scroll = $control.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern)
    $scroll.ScrollIntoView()
  }
  if (-not $control.GetCurrentPropertyValue([System.Windows.Automation.AutomationElement]::IsInvokePatternAvailableProperty)) {
    throw "UI control does not expose the accessible Invoke action: $name"
  }
  $pattern = $control.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
  $pattern.Invoke()
  Start-Sleep -Milliseconds 500
}
function Screenshot([string]$name) {
  # RNW 0.80's SetScrollPercent returns success without scrolling. Use the
  # implemented ScrollItem action on the header instead.
  $header = Find-Control 'app-header'
  if ($header) {
    $scroll = $header.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern)
    $scroll.ScrollIntoView()
    Start-Sleep -Milliseconds 500
  }
  $rect = [CaptureWindow+Rect]::new()
  [CaptureWindow]::GetWindowRect($script:app.MainWindowHandle,[ref]$rect) | Out-Null
  $bitmap = [Drawing.Bitmap]::new($rect.Right-$rect.Left,$rect.Bottom-$rect.Top)
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.CopyFromScreen($rect.Left,$rect.Top,0,0,$bitmap.Size)
    $bitmap.Save((Join-Path $screenshots "$name.png"),[Drawing.Imaging.ImageFormat]::Png)
  } finally { $graphics.Dispose(); $bitmap.Dispose() }
}
function Assert-Name([string]$id, [string]$expected) {
  $control = Find-Control $id
  if (-not $control -or $control.Current.Name -ne $expected) {
    $actual = if ($control) { $control.Current.Name } else { '<missing>' }
    throw "Unexpected text for $id : '$actual', expected '$expected'"
  }
}
function Start-App {
  $script:element = $null
  $script:app = Start-Process (Join-Path $destination 'RoClick.exe') -WorkingDirectory $destination -PassThru
  for ($i=0; $i -lt 90; $i++) {
    Start-Sleep -Milliseconds 500
    $script:app.Refresh()
    if ($script:app.HasExited) { throw "Installed app exited: $($script:app.ExitCode)" }
    if ($script:app.MainWindowHandle -ne 0) {
      $script:element = [System.Windows.Automation.AutomationElement]::FromHandle($script:app.MainWindowHandle)
      if (Find-Control 'capture') { break }
    }
  }
  if (-not (Find-Control 'capture')) { throw 'React UI did not load from bundled JavaScript.' }
  [CaptureWindow]::SetForegroundWindow($script:app.MainWindowHandle) | Out-Null
}
function Close-App {
  $script:app.CloseMainWindow() | Out-Null
  if (-not $script:app.WaitForExit(10000)) { throw 'App did not close cleanly.' }
}
function Assert-NoValidation {
  if (Find-Control 'validation') { throw 'Unedited fields require validation.' }
}
$script:element = $null
$script:app = $null
$receiver = $null
$settingsKey = 'HKCU:\Software\RoClick'
$previousLanguage = (Get-ItemProperty $settingsKey -Name Language -ErrorAction SilentlyContinue).Language
try {
  Remove-ItemProperty $settingsKey -Name Language -ErrorAction SilentlyContinue
  Install-App 'install'
  Start-App
  Click-Control 'language-en'
  Assert-Name 'capture' 'Capture window'
  # Verify edits and validation survive a live locale switch.
  $shell = New-Object -ComObject WScript.Shell
  (Find-Control 'interval-input').SetFocus()
  $shell.SendKeys('^a0')
  Start-Sleep -Milliseconds 500
  $shell.SendKeys('{ENTER}{TAB}')
  Start-Sleep -Milliseconds 500
  if (-not (Find-Control 'validation')) { throw 'Invalid interval was not rejected.' }
  Click-Control 'language-tr'
  Assert-Name 'validation' '1–1.000 ms arasında tam sayı bir aralık gir.'
  (Find-Control 'interval-input').SetFocus()
  $shell.SendKeys('^a25')
  Start-Sleep -Milliseconds 500
  $shell.SendKeys('{ENTER}{TAB}')
  Start-Sleep -Milliseconds 500
  Assert-NoValidation
  Click-Control 'button-right'
  Click-Control 'advanced'
  Click-Control 'language-en'
  Assert-Name 'press-input' 'Press duration in milliseconds'
  if ((Find-Control 'interval-input').GetCurrentPattern([System.Windows.Automation.ValuePattern]::Pattern).Current.Value -ne '25') {
    throw 'Language change reset the interval.'
  }
  if (-not (Find-Control 'button-right').GetCurrentPropertyValue([System.Windows.Automation.SelectionItemPattern]::IsSelectedProperty)) {
    throw 'Language change reset the mouse button.'
  }
  Click-Control 'advanced'
  # An attempted capture of RoClick itself produces a native error to translate.
  Click-Control 'capture'
  Start-Sleep -Milliseconds 2500
  if (-not (Find-Control 'engine-error')) { throw 'Expected a native capture error.' }
  Click-Control 'language-tr'
  Assert-Name 'engine-status' 'Durduruldu · işlem gerekiyor'
  Assert-Name 'engine-error' 'Bu pencere seçilemedi. Başka bir masaüstü uygulamasına geçip yeniden seç.'
  Click-Control 'capture'
  Click-Control 'primary-action'
  # Capture an independent window, then Start while RoClick is foreground:
  # focus lock must pause before any click can be submitted.
  $receiver = Start-Process notepad.exe -PassThru
  for ($i=0; $i -lt 40; $i++) {
    $receiver.Refresh()
    if ($receiver.MainWindowHandle -ne 0) { break }
    Start-Sleep -Milliseconds 250
  }
  if ($receiver.MainWindowHandle -eq 0) { throw 'Receiver window did not open.' }
  Click-Control 'capture'
  [CaptureWindow]::SetForegroundWindow($receiver.MainWindowHandle) | Out-Null
  Start-Sleep -Milliseconds 2500
  [CaptureWindow]::SetForegroundWindow($app.MainWindowHandle) | Out-Null
  Click-Control 'primary-action'
  Assert-Name 'engine-status' 'Duraklatıldı · hedef odakta değil'
  Click-Control 'language-en'
  Assert-Name 'engine-status' 'Paused · target not focused'
  # Low-level hotkeys deliberately ignore injected keys; exercise the UI Stop.
  Click-Control 'primary-action'
  Assert-Name 'engine-status' 'Ready when you are'
  Click-Control 'advanced'
  Click-Control 'clear-target'
  Click-Control 'advanced'
  $receiver.CloseMainWindow() | Out-Null
  $receiver.WaitForExit(5000) | Out-Null
  # A restart must retain the Turkish override.
  Click-Control 'language-tr'
  Close-App
  Start-App
  Assert-Name 'capture' 'Pencere seç'
  foreach ($locale in @('en','tr')) {
    Click-Control "language-$locale"
    foreach ($theme in @('light','dark')) {
      Click-Control 'advanced'
      Click-Control "theme-$theme"
      Click-Control 'advanced'
      Assert-NoValidation
      Screenshot "roclick-$locale-$theme"
    }
  }
  # Check a Turkish narrow viewport and retain an Advanced capture as evidence.
  [CaptureWindow]::MoveWindow($app.MainWindowHandle, 8, 8, 360, 640, $true) | Out-Null
  Start-Sleep -Milliseconds 500
  Screenshot 'roclick-tr-narrow'
  Click-Control 'advanced'
  $press = Find-Control 'press-input'
  $press.GetCurrentPattern([System.Windows.Automation.ScrollItemPattern]::Pattern).ScrollIntoView()
  Start-Sleep -Milliseconds 500
  # Capture the current scroll position for the expanded controls.
  $rect = [CaptureWindow+Rect]::new()
  [CaptureWindow]::GetWindowRect($app.MainWindowHandle, [ref]$rect) | Out-Null
  $bitmap = [Drawing.Bitmap]::new($rect.Right-$rect.Left, $rect.Bottom-$rect.Top)
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.CopyFromScreen($rect.Left,$rect.Top,0,0,$bitmap.Size)
    $bitmap.Save((Join-Path $screenshots 'roclick-tr-advanced.png'),[Drawing.Imaging.ImageFormat]::Png)
  } finally { $graphics.Dispose(); $bitmap.Dispose() }
  Close-App
  Install-App 'reinstall'
  $uninstall = Start-Process (Join-Path $destination 'unins000.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=`"$artifacts/uninstall.log`"") -Wait -PassThru
  if ($uninstall.ExitCode -ne 0 -or (Test-Path (Join-Path $destination 'RoClick.exe'))) { throw 'Uninstall failed.' }
  @{install='passed'; bundledLaunch='passed'; reinstall='passed'; uninstall='passed';
    localization='en/tr passed'; languagePersistence='passed'; nativeErrorTranslation='passed';
    pausedLanguageSwitch='passed'; validationLanguageSwitch='passed';
    cleanMachine='not tested'; versionUpgrade='not tested'; os=[Environment]::OSVersion.VersionString} |
    ConvertTo-Json | Set-Content (Join-Path $artifacts 'smoke-install.json')
} catch {
  if ($app -and -not $app.HasExited -and $app.MainWindowHandle -ne 0) {
    try { Screenshot 'smoke-failure' } catch { Write-Warning $_ }
  }
  throw
} finally {
  if ($app -and -not $app.HasExited) { Stop-Process -Id $app.Id -ErrorAction SilentlyContinue }
  if ($receiver -and -not $receiver.HasExited) { Stop-Process -Id $receiver.Id -ErrorAction SilentlyContinue }
  if ($previousLanguage) { Set-ItemProperty $settingsKey -Name Language -Value $previousLanguage }
  else { Remove-ItemProperty $settingsKey -Name Language -ErrorAction SilentlyContinue }
}
