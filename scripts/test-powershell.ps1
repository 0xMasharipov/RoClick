$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$files = @((Get-Item (Join-Path $root 'build.ps1'))) + @(Get-ChildItem $PSScriptRoot -Filter '*.ps1' -File -Recurse)
$failures = @()
foreach ($file in $files) {
  $tokens = $null
  $parseErrors = $null
  # Read UTF-8 explicitly, including when this check runs on Windows PowerShell.
  $null = [System.Management.Automation.Language.Parser]::ParseInput(
    [IO.File]::ReadAllText($file.FullName), $file.FullName, [ref]$tokens, [ref]$parseErrors)
  foreach ($parseError in $parseErrors) {
    $failures += "$($file.Name):$($parseError.Extent.StartLineNumber): $($parseError.Message)"
  }
}
if ($failures.Count) { throw ($failures -join [Environment]::NewLine) }
Write-Host "PowerShell syntax passed: $($files.Count) scripts."
