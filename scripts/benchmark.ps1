param([int]$Seconds = 60, [int]$LoadThreads = 0, [string]$Output = 'artifacts/benchmark')
$ErrorActionPreference = 'Stop'
New-Item $Output -ItemType Directory -Force | Out-Null
$metadata = @{
  recordedAt = (Get-Date).ToUniversalTime().ToString('o')
  os = Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber
  cpu = Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors
  computer = Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer,Model,TotalPhysicalMemory
  gitCommit = (git rev-parse HEAD)
  secondsPerRate = $Seconds
  loadThreads = $LoadThreads
  mode = $(if ($LoadThreads -gt 0) {'cpu-load'} else {'idle'})
  note = 'Interactive native-engine benchmark. UI-busy validation is separate. Received counts apply only to the local receiver.'
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content "$Output/metadata.json"
& ./build/native/Release/roclick_benchmark.exe $Seconds $LoadThreads > "$Output/results.json"
$result = $LASTEXITCODE
if ($result -eq 2) { throw 'Benchmark setup failed. Use an unlocked interactive desktop and keep the receiver focused.' }
if ($result -ne 0) { Write-Warning 'One or more performance targets missed. See results.json; do not present this as passing.' }
exit $result
