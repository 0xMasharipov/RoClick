param([switch]$SkipInstall)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Push-Location $PSScriptRoot
try {
  if (-not $IsWindows) { throw 'Release builds require x64 Windows and Visual Studio 2022. Use PowerShell 7.' }
  function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
  }
  if (-not $SkipInstall) { Invoke-Checked 'npm.cmd' @('ci', '--no-audit', '--no-fund') }
  Invoke-Checked 'npm.cmd' @('run', 'typecheck')
  Invoke-Checked 'cmake.exe' @('-S','native','-B','build/native','-A','x64')
  Invoke-Checked 'cmake.exe' @('--build','build/native','--config','Release')
  Invoke-Checked 'ctest.exe' @('--test-dir','build/native','-C','Release','--output-on-failure')
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if (-not $vs) { throw 'Install Visual Studio 2022 Desktop development with C++ and Windows 11 SDK 10.0.26100.0.' }
  $msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
  $solutionDir = (Resolve-Path 'windows').Path + '\'
  $output = Join-Path $PSScriptRoot 'build\app\'
  # The solution contains only the unpackaged desktop project. RNW's targets
  # require solution metadata even when there is just one app project.
  Invoke-Checked $msbuild @('windows/RoClick.sln','/restore','/m',
    '/p:Configuration=Release','/p:Platform=x64',"/p:SolutionDir=$solutionDir",
    '/p:UseBundle=true','/p:WindowsPackageType=None','/p:WindowsAppSDKSelfContained=true',
    "/p:OutDir=$output", '/bl:build/windows.binlog')
  $stage = Join-Path $PSScriptRoot 'build\stage'
  if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
  New-Item $stage -ItemType Directory -Force | Out-Null
  Get-ChildItem 'build/app' -Recurse -File | Where-Object { $_.Extension -notin '.pdb','.lib','.exp','.obj','.map' } | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath((Join-Path $PSScriptRoot 'build\app'), $_.FullName)
    $destination = Join-Path $stage $relative
    New-Item (Split-Path $destination) -ItemType Directory -Force | Out-Null
    Copy-Item $_.FullName $destination
  }
  Copy-Item 'windows/RoClick/Bundle' $stage -Recurse -Force
  Copy-Item 'assets/roclick.ico' $stage
  Copy-Item 'LICENSE' $stage
  # App-local MSVC runtime: offline install, no runtime download or UAC needed.
  $redist = Get-ChildItem (Join-Path $vs 'VC\Redist\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
  $crt = Join-Path $redist.FullName 'x64\Microsoft.VC143.CRT'
  if (-not (Test-Path $crt)) { throw 'MSVC x64 redistributable files are missing.' }
  Copy-Item "$crt\*.dll" $stage -Force
  foreach ($required in @('RoClick.exe','Microsoft.ReactNative.dll','Microsoft.UI.Xaml.dll','vcruntime140.dll','msvcp140.dll','Bundle/index.windows.bundle')) {
    if (-not (Test-Path (Join-Path $stage $required))) { throw "Incomplete runtime payload: $required" }
  }
  $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
  if (-not (Test-Path $iscc)) { throw 'Install Inno Setup 6 before building the installer.' }
  $version = (Get-Content package.json -Raw | ConvertFrom-Json).version
  Invoke-Checked $iscc @("/DAppVersion=$version", "/DSourceDir=$stage", 'installer/RoClick.iss')
  Copy-Item 'build/native/Release/roclick_benchmark.exe' 'dist/'
  @('RoClick-Setup.exe','roclick_benchmark.exe') | ForEach-Object {
    $hash = (Get-FileHash (Join-Path 'dist' $_) -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $_"
  } | Set-Content 'dist/SHA256SUMS.txt' -Encoding ascii
  Write-Host 'Prepared dist/RoClick-Setup.exe and SHA256SUMS.txt. Nothing has been published.'
} finally { Pop-Location }
