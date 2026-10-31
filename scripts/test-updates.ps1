$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'update.ps1')
$startInstaller = (Get-Command Start-UpdateInstaller).ScriptBlock

function Assert([bool]$Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Assert-Throws([scriptblock]$Action, [string]$Message) {
  $threw = $false
  try { & $Action } catch { $threw = $true }
  Assert $threw $Message
}
function New-Release([string]$Tag = 'v1.2.0') {
  $base = 'https://github.com/0xMasharipov/RoClick/releases/download/' + $Tag + '/'
  return [pscustomobject]@{
    tag_name=$Tag; draft=$false; prerelease=$false
    assets=@(
      [pscustomobject]@{name='RoClick-Setup.exe'; size=123; browser_download_url=($base + 'RoClick-Setup.exe')},
      [pscustomobject]@{name='SHA256SUMS.txt'; size=100; browser_download_url=($base + 'SHA256SUMS.txt')}
    )
  }
}

Assert ((Get-StableVersion 'v1.2.3') -eq [version]'1.2.3.0') 'Stable tag normalization failed.'
foreach ($invalid in @('', '1.2', '1.2.3.4', 'v1.2.3-beta', '1.2.3+build', '01.2.3', '2147483648.0.0', '1.2.3/../x')) {
  Assert ($null -eq (Get-StableVersion $invalid)) "Accepted invalid stable version: $invalid"
}
Assert ((Get-ReleaseUpdate (New-Release 'v1.10.0') '1.9.9').Version -eq [version]'1.10.0.0') 'Version comparison is not numeric.'
foreach ($tag in @('v1.0.0', 'v0.9.0', 'v1.2.0-beta', 'v01.2.0')) {
  Assert ($null -eq (Get-ReleaseUpdate (New-Release $tag) '1.0.0')) "Accepted old or unstable release: $tag"
}
$release = New-Release; $release.prerelease = $true
Assert ($null -eq (Get-ReleaseUpdate $release '1.0.0')) 'Accepted a prerelease.'
$release = New-Release; $release.draft = $true
Assert ($null -eq (Get-ReleaseUpdate $release '1.0.0')) 'Accepted a draft.'
$release = New-Release; $release.assets = @($release.assets[0])
Assert-Throws { Get-ReleaseUpdate $release '1.0.0' } 'Accepted missing checksums.'
$release = New-Release; $release.assets += $release.assets[0]
Assert-Throws { Get-ReleaseUpdate $release '1.0.0' } 'Accepted duplicate installers.'
foreach ($url in @('http://github.com/0xMasharipov/RoClick/releases/download/v1.2.0/RoClick-Setup.exe',
    'https://github.com/another/repo/releases/download/v1.2.0/RoClick-Setup.exe',
    'https://github.com.evil.test/0xMasharipov/RoClick/releases/download/v1.2.0/RoClick-Setup.exe',
    'https://github.com/0xMasharipov/RoClick/releases/download/v1.1.0/RoClick-Setup.exe')) {
  $release = New-Release; $release.assets[0].browser_download_url = $url
  Assert-Throws { Get-ReleaseUpdate $release '1.0.0' } "Accepted unexpected installer URL: $url"
}
foreach ($size in @(0, -1, 536870913)) {
  $release = New-Release; $release.assets[0].size = $size
  Assert-Throws { Get-ReleaseUpdate $release '1.0.0' } 'Accepted invalid download size.'
}
$hash = 'ab' * 32
Assert ((Get-ExpectedHash "$hash  RoClick-Setup.exe`r`n$('00' * 32)  roclick_benchmark.exe`r`n") -eq $hash) 'Checksum selection failed.'
Assert ((Get-ExpectedHash "$($hash.ToUpperInvariant()) *RoClick-Setup.exe") -eq $hash) 'Binary checksum parsing failed.'
foreach ($checksums in @('', "$hash  other.exe", "$hash  RoClick-Setup.exe`n$hash  RoClick-Setup.exe", "$('a' * 63)  RoClick-Setup.exe")) {
  Assert-Throws { Get-ExpectedHash $checksums } 'Accepted missing or ambiguous checksums.'
}

$temporary = Join-Path ([IO.Path]::GetTempPath()) ('RoClick-updater-tests-' + [guid]::NewGuid().ToString('N'))
New-Item $temporary -ItemType Directory | Out-Null
try {
  $installer = Join-Path $temporary 'RoClick-Setup.exe'
  $versionFile = Join-Path $temporary 'version.txt'
  [IO.File]::WriteAllText($installer, 'offline fixture, never executed')
  [IO.File]::WriteAllText($versionFile, '1.0.0')
  $expected = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
  $script:installerVersion = [version]'1.2.0.0'
  $script:installCalls = 0
  $script:advanceVersion = $true
  # Only the process launch and Windows file-version resource are replaced.
  # Hash verification and the installed-version guards use real fixture files.
  function Get-InstallerVersion([string]$Installer) { return $script:installerVersion }
  function Start-UpdateInstaller([string]$Installer, [string]$Directory) {
    $script:installCalls++
    if ($script:advanceVersion) { [IO.File]::WriteAllText((Join-Path $Directory 'version.txt'), '1.2.0') }
  }
  Assert (Install-VerifiedUpdate $installer $expected ([version]'1.2.0.0') $temporary) 'Verified update was not installed.'
  Assert ($script:installCalls -eq 1) 'Installer did not run exactly once.'
  Assert (-not (Install-VerifiedUpdate $installer $expected ([version]'1.2.0.0') $temporary)) 'Same version was reinstalled.'
  [IO.File]::WriteAllText($versionFile, '2.0.0')
  Assert (-not (Install-VerifiedUpdate $installer $expected ([version]'1.2.0.0') $temporary)) 'A newer manual install was downgraded.'
  Assert ($script:installCalls -eq 1) 'Unnecessary installer execution.'
  [IO.File]::WriteAllText($versionFile, '1.0.0')
  $script:installerVersion = [version]'9.9.9.0'
  Assert-Throws { Install-VerifiedUpdate $installer $expected ([version]'1.2.0.0') $temporary } 'Accepted an installer for a different version.'
  $script:installerVersion = [version]'1.2.0.0'
  [IO.File]::WriteAllText($installer, 'modified after download')
  Assert-Throws { Install-VerifiedUpdate $installer $expected ([version]'1.2.0.0') $temporary } 'Executed a modified installer.'
  Assert ($script:installCalls -eq 1) 'Unverified installer was executed.'
  $expected = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash
  $script:advanceVersion = $false
  Assert-Throws { Install-VerifiedUpdate $installer $expected ([version]'1.2.0.0') $temporary } 'Reported success without an installed version change.'
  [IO.File]::WriteAllText($versionFile, 'invalid')
  Assert-Throws { Install-VerifiedUpdate $installer $expected ([version]'1.2.0.0') $temporary } 'Installed over an unknown version.'

  # Exercise the helper loop without network or executable launches. The real
  # version/hash/install guards remain active; process and download boundaries
  # are supplied by fixtures, including a second still-open app instance.
  $script:AppDirectory = $temporary
  $script:CurrentVersion = '1.0.0'
  $script:StateDirectory = Join-Path $temporary 'job'
  New-Item $script:StateDirectory -ItemType Directory | Out-Null
  $script:downloads = 0
  $script:enabledChecks = 0
  $script:enabledMode = 'on'
  $script:otherWindows = 0
  function Get-UpdateParent {
    $parent = [pscustomobject]@{HasExited=$false}
    $parent | Add-Member ScriptMethod WaitForExit {
      param($Milliseconds)
      Assert ($script:installCalls -eq 0) 'Installed while the original app was still open.'
      $this.HasExited = $true
      return $true
    }
    $parent | Add-Member ScriptMethod Dispose {}
    return $parent
  }
  function Test-UpdatesEnabled {
    $script:enabledChecks++
    return $script:enabledMode -eq 'on' -or ($script:enabledMode -eq 'disablePending' -and $script:enabledChecks -eq 1)
  }
  function Get-RunningApps([string]$Directory) {
    $script:otherWindows++
    if ($script:otherWindows -eq 1) { return @('another open app') }
    return @()
  }
  function Start-Sleep { param($Seconds) Assert ($script:installCalls -eq 0) 'Installed while another app window was open.' }
  function Save-UpdateDownload([string]$Url, [string]$Destination, [long]$MaximumBytes) {
    $script:downloads++
    $content = 'verified update fixture'
    if ($Url.EndsWith('/latest')) {
      $release = New-Release
      $release.assets[0].size = [Text.Encoding]::UTF8.GetByteCount($content)
      [IO.File]::WriteAllText($Destination, ($release | ConvertTo-Json -Depth 5))
    } elseif ($Url.EndsWith('/SHA256SUMS.txt')) {
      $sha = [Security.Cryptography.SHA256]::Create()
      try { $digest = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($content))).Replace('-', '') }
      finally { $sha.Dispose() }
      [IO.File]::WriteAllText($Destination, "$digest  RoClick-Setup.exe")
    } else { [IO.File]::WriteAllText($Destination, $content) }
  }
  foreach ($mode in @('on', 'off', 'disablePending')) {
    $script:enabledMode = $mode; $script:enabledChecks = 0; $script:downloads = 0
    $script:installCalls = 0; $script:otherWindows = 0; $script:advanceVersion = $true
    [IO.File]::WriteAllText($versionFile, '1.0.0')
    [IO.File]::WriteAllText((Join-Path $temporary 'RoClick.exe'), 'app fixture')
    Start-AutomaticUpdates
    if ($mode -eq 'on') {
      Assert ($script:downloads -eq 3) 'Did not fetch metadata, checksums and installer.'
      Assert ($script:installCalls -eq 1) 'Pending update was not installed after exit.'
      Assert ($script:otherWindows -eq 2) 'Did not wait for the other open app window.'
    } else {
      Assert ($script:installCalls -eq 0) 'Installed with automatic updates disabled.'
      if ($mode -eq 'off') { Assert ($script:downloads -eq 0) 'Contacted the network with updates disabled.' }
    }
  }
  # Exercise the real installer launch boundary and synchronization with only
  # Start-Process replaced, so no fixture is executed on the test machine.
  $script:enabledMode = 'on'; $script:otherWindows = 1
  $script:exitCode = 0; $script:processLaunches = 0
  function Start-Process {
    param($FilePath, $ArgumentList, [switch]$Wait, [switch]$PassThru)
    $script:processLaunches++
    Assert ($FilePath -eq $installer) 'Unexpected executable launch.'
    foreach ($flag in @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/NOCLOSEAPPLICATIONS', '/NORESTARTAPPLICATIONS')) {
      Assert ($ArgumentList -contains $flag) "Missing unattended installation flag: $flag"
    }
    Assert ($ArgumentList -contains ('/DIR="' + $temporary + '"')) 'Installation directory was not preserved.'
    Assert ($Wait -and $PassThru) 'Did not wait for the installer result.'
    return [pscustomobject]@{ExitCode=$script:exitCode}
  }
  & $startInstaller $installer $temporary
  Assert ($script:processLaunches -eq 1) 'Installer boundary was not called.'
  $script:exitCode = 7
  Assert-Throws { & $startInstaller $installer $temporary } 'Ignored an installer failure.'
  $script:otherWindows = 0
  Assert-Throws { & $startInstaller $installer $temporary } 'Installed after another app window reopened.'
  Assert ($script:processLaunches -eq 2) 'Launched while a window was open.'
  $script:enabledMode = 'off'
  Assert-Throws { & $startInstaller $installer $temporary } 'Installed after updates were disabled while waiting for the install gate.'
  Assert ($script:processLaunches -eq 2) 'Launched while updates were disabled.'
} finally { Remove-Item -LiteralPath $temporary -Recurse -Force }
Write-Host 'Updater passed: stable releases, origin, asset limits, tampering, downgrade prevention, install results, defer until all windows close, disable and cancel pending.'
