# Exercise real installed apps, native modules, helpers and installers. Only the
# download boundary is replaced with local release assets; no test hooks ship.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$root = Split-Path $PSScriptRoot
$temporary = Join-Path $env:TEMP ('RoClick-upgrade-' + [guid]::NewGuid().ToString('N'))
$destination = Join-Path $temporary 'installed'
$fixture = Join-Path $temporary 'release'
$settings = 'HKCU:\Software\RoClick'
$saved = @{}
$apps = [Collections.Generic.List[Diagnostics.Process]]::new()
$helpers = [Collections.Generic.List[Diagnostics.Process]]::new()
New-Item $fixture -ItemType Directory -Force | Out-Null

function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Wait-Until([scriptblock]$Condition, [string]$Message, [int]$Seconds = 60) {
  $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
  do {
    if (& $Condition) { return }
    Start-Sleep -Milliseconds 200
  } while ([DateTime]::UtcNow -lt $deadline)
  throw $Message
}
function Find-Control([string]$Id) {
  $condition = [System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::AutomationIdProperty, $Id)
  return [System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Descendants, $condition)
}
function Click-Control([string]$Id) {
  $control = Find-Control $Id
  Assert ($null -ne $control) "Missing control: $Id"
  $control.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
  Start-Sleep -Milliseconds 300
}
function Install-App([string]$Installer) {
  $process = Start-Process $Installer -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/DIR="' + $destination + '"')) -Wait -PassThru
  Assert ($process.ExitCode -eq 0) 'Test installer failed.'
}
function Set-Fixture([string]$Version, [string]$Installer) {
  Copy-Item $Installer (Join-Path $fixture 'RoClick-Setup.exe') -Force
  $hash = (Get-FileHash (Join-Path $fixture 'RoClick-Setup.exe') -Algorithm SHA256).Hash
  [IO.File]::WriteAllText((Join-Path $fixture 'SHA256SUMS.txt'), "$hash  RoClick-Setup.exe")
  $base = "https://github.com/0xMasharipov/RoClick/releases/download/v$Version/"
  @{
    tag_name="v$Version"; draft=$false; prerelease=$false
    assets=@(
      @{name='RoClick-Setup.exe'; size=(Get-Item (Join-Path $fixture 'RoClick-Setup.exe')).Length; browser_download_url=($base+'RoClick-Setup.exe')},
      @{name='SHA256SUMS.txt'; size=(Get-Item (Join-Path $fixture 'SHA256SUMS.txt')).Length; browser_download_url=($base+'SHA256SUMS.txt')}
    )
  } | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $fixture 'release.json') -Encoding ascii
  Copy-Item (Join-Path $destination 'update.ps1') (Join-Path $destination 'update-original.ps1') -Force
  $wrapper = @'
param([string]$AppDirectory, [string]$CurrentVersion, [int]$ParentProcessId, [long]$ParentStarted, [string]$StateDirectory)
$fixtureDirectory = __FIXTURE_DIRECTORY__
. (Join-Path $AppDirectory 'update-original.ps1') @PSBoundParameters
function Save-UpdateDownload([string]$Url, [string]$Destination, [long]$MaximumBytes) {
  $name = if ($Url.EndsWith('/latest')) { 'release.json' } else { ([uri]$Url).Segments[-1] }
  [IO.File]::Copy((Join-Path $fixtureDirectory $name), $Destination, $true)
}
Start-AutomaticUpdates
'@
  $literal = "'" + $fixture.Replace("'", "''") + "'"
  [IO.File]::WriteAllText((Join-Path $destination 'update.ps1'), $wrapper.Replace('__FIXTURE_DIRECTORY__', $literal), [Text.Encoding]::ASCII)
}
function Start-App {
  $process = Start-Process (Join-Path $destination 'RoClick.exe') -WorkingDirectory $destination -PassThru
  $apps.Add($process)
  Wait-Until { $process.Refresh(); -not $process.HasExited -and $process.MainWindowHandle -ne 0 } 'App did not open.'
  Wait-Until { $null -ne (Find-Control 'primary-action') } 'React UI did not load.'
  return $process
}
function Track-Helper([Diagnostics.Process]$Parent) {
  $script:helperId = $null
  Wait-Until {
    $found = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" | Where-Object {
      $_.CommandLine -match ('-ParentProcessId ' + $Parent.Id + '(?:\s|$)')
    })
    if ($found.Count) { $script:helperId = [int]$found[0].ProcessId; return $true }
    return $false
  } 'Update helper did not start.'
  $process = [Diagnostics.Process]::GetProcessById($script:helperId)
  $helpers.Add($process)
  return $process
}
function Wait-Version([string]$Version) {
  Wait-Until {
    $path = Join-Path $destination 'version.txt'
    (Test-Path $path) -and [IO.File]::ReadAllText($path).Trim() -eq $Version
  } "Installed version did not advance to $Version." 120
}
try {
  New-Item $settings -Force | Out-Null
  foreach ($name in @('AutomaticUpdates', 'Language')) {
    $saved[$name] = Get-ItemPropertyValue $settings -Name $name -ErrorAction SilentlyContinue
  }
  Set-ItemProperty $settings -Name AutomaticUpdates -Value 1 -Type DWord
  Set-ItemProperty $settings -Name Language -Value 'en'
  $version = (Get-Content (Join-Path $root 'package.json') -Raw | ConvertFrom-Json).version
  $next = [version]$version
  $futureVersion = '{0}.{1}.{2}' -f $next.Major, $next.Minor, ($next.Build + 1)

  # A real previous release exercises the updater already installed by users.
  $oldInstaller = Join-Path $temporary 'previous.exe'
  $oldChecksums = Join-Path $temporary 'previous-checksums.txt'
  Invoke-WebRequest 'https://github.com/0xMasharipov/RoClick/releases/download/v1.1.0/RoClick-Setup.exe' -OutFile $oldInstaller
  Invoke-WebRequest 'https://github.com/0xMasharipov/RoClick/releases/download/v1.1.0/SHA256SUMS.txt' -OutFile $oldChecksums
  $originalHelper = Join-Path $root 'scripts/update.ps1'
  . $originalHelper
  $expected = Get-ExpectedHash (Get-Content $oldChecksums -Raw)
  Assert ((Get-FileHash $oldInstaller -Algorithm SHA256).Hash -ieq $expected) 'Previous release checksum mismatch.'
  Install-App $oldInstaller
  Set-Fixture $version (Join-Path $root 'dist/RoClick-Setup.exe')
  $app = Start-App
  $helper = Track-Helper $app
  Wait-Until { $notice = Find-Control 'update-notice'; $notice -and $notice.Current.Name.Contains('is ready') } 'Previous updater did not stage the candidate.'
  Assert (-not $app.HasExited) 'Updater closed the previous app without consent.'
  Assert ($app.CloseMainWindow()) 'Could not close previous app normally.'
  Assert ($app.WaitForExit(15000)) 'Previous app did not close.'
  Wait-Version $version
  Assert ($helper.WaitForExit(30000)) 'Previous helper did not exit after installation.'

  # Build an unpublished future fixture from the candidate payload. Only its
  # version file and installer version differ; fixture assets are never uploaded.
  $stage = Join-Path $temporary 'future-stage'
  Copy-Item (Join-Path $root 'build/stage') $stage -Recurse
  [IO.File]::WriteAllText((Join-Path $stage 'version.txt'), $futureVersion, [Text.Encoding]::ASCII)
  $futureOutput = Join-Path $temporary 'future-output'
  New-Item $futureOutput -ItemType Directory | Out-Null
  $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
  & $iscc "/DAppVersion=$futureVersion" "/DSourceDir=$stage" "/O$futureOutput" (Join-Path $root 'installer/RoClick.iss')
  Assert ($LASTEXITCODE -eq 0) 'Future fixture build failed.'
  Set-Fixture $futureVersion (Join-Path $futureOutput 'RoClick-Setup.exe')
  $app = Start-App
  $helper = Track-Helper $app
  Wait-Until { $null -ne (Find-Control 'update-dialog') } 'Verified update did not show a popup.'
  Assert ((Find-Control 'update-dialog-title').Current.Name -eq "RoClick $futureVersion is ready to install") 'Wrong popup version or language.'
  Click-Control 'update-later'
  Start-Sleep -Seconds 7
  Assert ($null -eq (Find-Control 'update-dialog')) 'Later immediately prompted again.'
  Assert (-not $app.HasExited) 'Later closed the app.'
  Click-Control 'language-tr'
  Click-Control 'open-update'
  Assert ((Find-Control 'update-dialog-title').Current.Name.Contains($futureVersion)) 'Language switch lost the update.'
  Click-Control 'update-later'
  Click-Control 'language-en'
  Click-Control 'open-update'
  Set-ItemProperty $settings -Name AutomaticUpdates -Value 0 -Type DWord
  Wait-Until { $null -eq (Find-Control 'update-dialog') } 'Disabling updates did not dismiss the popup.'
  Assert (-not $app.HasExited) 'Disabling updates closed the app.'
  Set-ItemProperty $settings -Name AutomaticUpdates -Value 1 -Type DWord
  Wait-Until { $null -ne (Find-Control 'open-update') } 'Ready update did not return after enabling updates.'
  $second = Start-App
  Wait-Until { $null -ne (Find-Control 'update-dialog') } 'Second instance did not report the owning helper update.'
  Click-Control 'update-install'
  Assert ($app.WaitForExit(15000) -and $second.WaitForExit(15000)) 'Install now did not close all matching windows.'
  Wait-Version $futureVersion
  Assert ($helper.WaitForExit(30000)) 'Candidate helper did not finish installation.'
  Assert ((Get-ItemPropertyValue $settings -Name Language) -eq 'en') 'Upgrade changed the language preference.'
  Assert ((Get-ItemPropertyValue $settings -Name AutomaticUpdates) -eq 1) 'Upgrade changed the update preference.'
  $uninstall = Start-Process (Join-Path $destination 'unins000.exe') -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART') -Wait -PassThru
  Assert ($uninstall.ExitCode -eq 0) 'Fixture uninstall failed.'
  @{
    previousVersion='1.1.0'; candidateVersion=$version; privateFutureFixture=$futureVersion
    previousAutomaticUpgrade=$true; popup=$true; later=$true; languageSwitch=$true
    disablePending=$true; multiWindowInstallNow=$true; preferencePersistence=$true
    note='Real native apps/helpers/installers with local downloads. Future fixture is unpublished. Windows Server CI is not Windows 10/11 gameplay validation.'
  } | ConvertTo-Json | Set-Content (Join-Path $root 'artifacts/upgrade-tests.json')
} finally {
  foreach ($process in $apps) { if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue } }
  foreach ($process in $helpers) { if (-not $process.HasExited) { Stop-Process -Id $process.Id -ErrorAction SilentlyContinue } }
  foreach ($name in $saved.Keys) {
    if ($null -eq $saved[$name]) { Remove-ItemProperty $settings -Name $name -ErrorAction SilentlyContinue }
    else { Set-ItemProperty $settings -Name $name -Value $saved[$name] }
  }
  Remove-Item $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
