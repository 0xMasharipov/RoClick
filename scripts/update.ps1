param(
  [string]$AppDirectory,
  [string]$CurrentVersion,
  [int]$ParentProcessId,
  [long]$ParentStarted,
  [string]$StateDirectory
)

# Runs with the Windows PowerShell included in Windows 10/11. No scheduled task,
# service, administrator access or persistent execution-policy change is needed.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-StableVersion([string]$Value) {
  if ($Value -cnotmatch '^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { return $null }
  try { return [version]::new([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], 0) }
  catch { return $null }
}

function Get-ReleaseUpdate($Release, [string]$InstalledVersion) {
  $installed = Get-StableVersion $InstalledVersion
  if (-not $installed) { throw 'Invalid installed version.' }
  if ($Release.draft -or $Release.prerelease) { return $null }
  $version = Get-StableVersion $Release.tag_name
  if (-not $version -or $version -le $installed) { return $null }
  $installers = @($Release.assets | Where-Object { $_.name -ceq 'RoClick-Setup.exe' })
  $checksums = @($Release.assets | Where-Object { $_.name -ceq 'SHA256SUMS.txt' })
  if ($installers.Count -ne 1 -or $checksums.Count -ne 1) { throw 'Release assets are incomplete or ambiguous.' }
  $base = 'https://github.com/0xMasharipov/RoClick/releases/download/' + $Release.tag_name + '/'
  if ($installers[0].browser_download_url -cne ($base + 'RoClick-Setup.exe') -or
      $checksums[0].browser_download_url -cne ($base + 'SHA256SUMS.txt')) { throw 'Unexpected release asset URL.' }
  if ($installers[0].size -le 0 -or $installers[0].size -gt 536870912 -or
      $checksums[0].size -le 0 -or $checksums[0].size -gt 1048576) { throw 'Invalid release asset size.' }
  return @{Version=$version; Installer=$installers[0]; Checksums=$checksums[0]}
}

function Get-ExpectedHash([string]$Checksums) {
  $hashes = @([regex]::Matches($Checksums, '(?m)^([a-fA-F0-9]{64})[ \t]+\*?RoClick-Setup\.exe[ \t]*\r?$'))
  if ($hashes.Count -ne 1) { throw 'Missing or ambiguous installer checksum.' }
  return $hashes[0].Groups[1].Value.ToLowerInvariant()
}

function Save-UpdateDownload([string]$Url, [string]$Destination, [long]$MaximumBytes) {
  if ([uri]$Url -and ([uri]$Url).Scheme -ne 'https') { throw 'HTTPS is required.' }
  $request = [System.Net.HttpWebRequest]::Create($Url)
  $request.UserAgent = 'RoClick/' + $CurrentVersion
  $request.Accept = 'application/vnd.github+json'
  $request.Timeout = 15000
  $request.ReadWriteTimeout = 15000
  $request.MaximumAutomaticRedirections = 5
  $response = $null; $inputStream = $null; $outputStream = $null
  try {
    $response = $request.GetResponse()
    if ($response.ResponseUri.Scheme -ne 'https' -or $response.ContentLength -gt $MaximumBytes) { throw 'Invalid download response.' }
    $inputStream = $response.GetResponseStream()
    $outputStream = [IO.File]::Open($Destination, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $buffer = New-Object byte[] 65536
    [long]$total = 0
    $deadline = [DateTime]::UtcNow.AddMinutes(5)
    while (($count = $inputStream.Read($buffer, 0, $buffer.Length)) -gt 0) {
      $total += $count
      if ($total -gt $MaximumBytes -or [DateTime]::UtcNow -gt $deadline) { throw 'Download limit exceeded.' }
      $outputStream.Write($buffer, 0, $count)
    }
  } finally {
    if ($outputStream) { $outputStream.Dispose() }
    if ($inputStream) { $inputStream.Dispose() }
    if ($response) { $response.Dispose() }
  }
}

function Write-UpdateState([string]$Phase, [string]$AvailableVersion = '') {
  $temporary = Join-Path $StateDirectory 'state.tmp'
  [IO.File]::WriteAllText($temporary, "$Phase`n$AvailableVersion", [Text.Encoding]::ASCII)
  Move-Item -LiteralPath $temporary -Destination (Join-Path $StateDirectory 'state.txt') -Force
}

function Test-UpdatesEnabled {
  # Registry-provider missing-property errors can terminate under strict mode
  # despite SilentlyContinue. A clean install must default to enabled.
  $setting = [Microsoft.Win32.Registry]::GetValue('HKEY_CURRENT_USER\Software\RoClick', 'AutomaticUpdates', $null)
  return $null -eq $setting -or $setting -ne 0
}

function Get-InstallerVersion([string]$Installer) {
  return [version]([Diagnostics.FileVersionInfo]::GetVersionInfo($Installer).FileVersion)
}

function Start-UpdateInstaller([string]$Installer, [string]$Directory) {
  # Native startup uses this gate too: app files cannot open halfway through
  # replacement. Recheck under the gate in case a window reopened while waiting.
  $gate = [Threading.Mutex]::new($false, 'Local\RoClick.Installation')
  $owned = $false
  try {
    try { $owned = $gate.WaitOne(30000) }
    catch [Threading.AbandonedMutexException] { $owned = $true }
    if (-not $owned) { throw 'Another installation is in progress.' }
    if (-not (Test-UpdatesEnabled)) { throw 'Automatic updates were disabled before installation.' }
    if (@(Get-RunningApps $Directory).Count -gt 0) { throw 'RoClick reopened before installation; retry on next launch.' }
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/SP-', '/NORESTART',
      '/NOCLOSEAPPLICATIONS', '/NORESTARTAPPLICATIONS', '/RESTARTEXITCODE=3010', ('/DIR="' + $Directory + '"'))
    $process = Start-Process -FilePath $Installer -ArgumentList $arguments -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode)" }
  } finally {
    if ($owned) { $gate.ReleaseMutex() }
    $gate.Dispose()
  }
}

function Install-VerifiedUpdate([string]$Installer, [string]$ExpectedHash, [version]$Version, [string]$Directory) {
  # Recheck immediately before execution, including after waiting for app exit.
  if ((Get-FileHash -LiteralPath $Installer -Algorithm SHA256).Hash -ine $ExpectedHash) { throw 'Installer checksum mismatch.' }
  if ((Get-InstallerVersion $Installer) -ne $Version) { throw 'Installer version mismatch.' }
  $installed = Get-StableVersion ([IO.File]::ReadAllText((Join-Path $Directory 'version.txt')).Trim())
  if (-not $installed) { throw 'Installed version is unavailable.' }
  if ($Version -le $installed) { return $false }
  Start-UpdateInstaller $Installer $Directory
  $after = Get-StableVersion ([IO.File]::ReadAllText((Join-Path $Directory 'version.txt')).Trim())
  if (-not $after -or $after -lt $Version) { throw 'Installed version did not advance.' }
  return $true
}

function Get-RunningApps([string]$Directory) {
  $expected = Join-Path $Directory 'RoClick.exe'
  return @(Get-Process -Name RoClick -ErrorAction SilentlyContinue | Where-Object {
    try { $_.Path -ieq $expected } catch { $true }
  })
}

function Get-UpdateParent {
  $parent = [Diagnostics.Process]::GetProcessById($ParentProcessId)
  if ($parent.StartTime.ToFileTimeUtc() -ne $ParentStarted -or
      $parent.MainModule.FileName -ine (Join-Path $AppDirectory 'RoClick.exe')) {
    $parent.Dispose()
    throw 'Application identity mismatch.'
  }
  return $parent
}

function Start-AutomaticUpdates {
  if (-not (Get-StableVersion $CurrentVersion)) { throw 'Invalid application version.' }
  $parent = Get-UpdateParent
  # One helper per installation. Other app instances leave this helper in charge.
  $lock = $null
  try {
    try { $lock = [IO.File]::Open((Join-Path (Split-Path $StateDirectory) 'update.lock'),
        [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None) }
    catch { Write-UpdateState 'otherInstance'; return }
    [IO.File]::WriteAllText((Join-Path (Split-Path $StateDirectory) 'active-job.txt'), (Split-Path $StateDirectory -Leaf), [Text.Encoding]::ASCII)
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $nextCheck = [DateTime]::MinValue
    $wasEnabled = $false
    while (-not $parent.HasExited) {
      if (-not (Test-UpdatesEnabled)) {
        Write-UpdateState 'disabled'
        $wasEnabled = $false
      } else {
        if (-not $wasEnabled) { $nextCheck = [DateTime]::MinValue; $wasEnabled = $true }
        if ([DateTime]::UtcNow -ge $nextCheck) {
          try {
            Write-UpdateState 'checking'
            $metadata = Join-Path $StateDirectory 'release.json'
            Save-UpdateDownload 'https://api.github.com/repos/0xMasharipov/RoClick/releases/latest' $metadata 1048576
            $update = Get-ReleaseUpdate (Get-Content -LiteralPath $metadata -Raw | ConvertFrom-Json) $CurrentVersion
            if ($update) {
              $versionText = $update.Version.ToString(3)
              Write-UpdateState 'downloading' $versionText
              $checksumPath = Join-Path $StateDirectory 'SHA256SUMS.txt'
              $installer = Join-Path $StateDirectory 'RoClick-Setup.exe'
              Save-UpdateDownload $update.Checksums.browser_download_url $checksumPath 1048576
              $expectedHash = Get-ExpectedHash (Get-Content -LiteralPath $checksumPath -Raw)
              Save-UpdateDownload $update.Installer.browser_download_url $installer 536870912
              if ((Get-Item -LiteralPath $installer).Length -ne $update.Installer.size -or
                  (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ine $expectedHash) { throw 'Installer verification failed.' }
              if ((Get-InstallerVersion $installer) -ne $update.Version) { throw 'Installer version mismatch.' }
              # Wait on the original process object, so a reused PID cannot trigger an install.
              while (-not $parent.HasExited) {
                Write-UpdateState $(if (Test-UpdatesEnabled) { 'ready' } else { 'disabled' }) $versionText
                $null = $parent.WaitForExit(1000)
              }
              if (-not (Test-UpdatesEnabled)) { return }
              # Multiple open windows must all finish before files are replaced.
              while (@(Get-RunningApps $AppDirectory).Count -gt 0) {
                if (-not (Test-UpdatesEnabled)) { return }
                Start-Sleep -Seconds 1
              }
              if (-not (Test-UpdatesEnabled)) { return }
              if (-not (Test-Path -LiteralPath (Join-Path $AppDirectory 'RoClick.exe'))) { return }
              $null = Install-VerifiedUpdate $installer $expectedHash $update.Version $AppDirectory
              return
            }
            Write-UpdateState 'current'
            $nextCheck = [DateTime]::UtcNow.AddHours(6)
          } catch {
            Write-UpdateState 'error'
            [IO.File]::WriteAllText((Join-Path (Split-Path $StateDirectory) 'last-error.txt'), $_.Exception.Message)
            $nextCheck = [DateTime]::UtcNow.AddMinutes(15)
          }
        }
      }
      $null = $parent.WaitForExit(1000)
    }
  } finally {
    if ($lock) { $lock.Dispose() }
    $parent.Dispose()
  }
}

# Dot-sourcing exposes the policy and verification functions to offline tests.
if ($MyInvocation.InvocationName -ne '.') {
  try { Start-AutomaticUpdates }
  catch { if ($StateDirectory -and (Test-Path -LiteralPath $StateDirectory)) { Write-UpdateState 'error' } }
  finally {
    # Each directory is a private, generated job directory, never an app path.
    # Keep state while the app is alive so it can report failures; clean next launch.
    if ($StateDirectory -and -not (Get-Process -Id $ParentProcessId -ErrorAction SilentlyContinue)) {
      Remove-Item -LiteralPath $StateDirectory -Recurse -Force -ErrorAction SilentlyContinue
    }
  }
}
