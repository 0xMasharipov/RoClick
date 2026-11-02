param([int]$Seconds = 10, [int]$Repeats = 3, [string]$Baseline = 'df440cb', [string]$Output = 'artifacts/performance')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot
Push-Location $root
try {
  $outputPath = [IO.Path]::GetFullPath((Join-Path $root $Output))
  New-Item $outputPath -ItemType Directory -Force | Out-Null
  $source = Join-Path $root 'build/performance-baseline'
  New-Item $source -ItemType Directory -Force | Out-Null
  $archive = Join-Path $source 'native.tar'
  & git archive --format=tar --output=$archive $Baseline native
  if ($LASTEXITCODE -ne 0) { throw 'Could not read baseline sources.' }
  & tar -xf $archive -C $source
  if ($LASTEXITCODE -ne 0) { throw 'Could not extract baseline sources.' }
  # Both versions use the identical measurement harness, receiver, rates and waits.
  Copy-Item 'native/tests/Benchmark.cpp' (Join-Path $source 'native/tests/Benchmark.cpp') -Force
  # Identical read-only counter accessors; the legacy worker is untouched.
  $header = Join-Path $source 'native/windows/Runtime.h'
  $counter = @'
  RuntimeSnapshot Read();
  uint64_t WorkerCycles() {
    ULONG64 cycles = 0;
    QueryThreadCycleTime(worker_.native_handle(), &cycles);
    return cycles;
  }
'@
  $text = [IO.File]::ReadAllText($header)
  if (-not $text.Contains('  RuntimeSnapshot Read();')) { throw 'Baseline counter insertion point not found.' }
  [IO.File]::WriteAllText($header, $text.Replace('  RuntimeSnapshot Read();', $counter))
  & cmake -S (Join-Path $source 'native') -B (Join-Path $source 'out') -A x64
  if ($LASTEXITCODE -ne 0) { throw 'Baseline configuration failed.' }
  & cmake --build (Join-Path $source 'out') --config Release --target roclick_benchmark
  if ($LASTEXITCODE -ne 0) { throw 'Baseline compilation failed.' }
  $executables = @{
    baseline = Join-Path $source 'out/Release/roclick_benchmark.exe'
    candidate = Join-Path $root 'build/native/Release/roclick_benchmark.exe'
  }
  $results = @{baseline=@(); candidate=@()}
  for ($trial = 1; $trial -le $Repeats; $trial++) {
    # Alternate order to limit systematic warm-up/host-load bias.
    $order = if ($trial % 2) { @('baseline', 'candidate') } else { @('candidate', 'baseline') }
    foreach ($name in $order) {
      $file = Join-Path $outputPath "$name-$trial.json"
      & $executables[$name] $Seconds 0 > $file
      $code = $LASTEXITCODE
      if ($code -eq 2) { throw "$name benchmark setup failed; requires an interactive desktop." }
      $result = Get-Content $file -Raw | ConvertFrom-Json
      if ($result.samples.Count -ne 4) { throw 'Incomplete benchmark output.' }
      if ($name -eq 'candidate' -and -not $result.gameMode.passed) { throw 'Native game-friendly limit or stop check failed.' }
      $results[$name] += $result
    }
  }
  function Median($Values) {
    $sorted = @($Values | Sort-Object)
    return $sorted[[int][math]::Floor($sorted.Count / 2)]
  }
  $comparison = foreach ($cps in @(10, 100, 500, 1000)) {
    $before = @($results.baseline | ForEach-Object { $_.samples | Where-Object configuredCps -eq $cps })
    $after = @($results.candidate | ForEach-Object { $_.samples | Where-Object configuredCps -eq $cps })
    $beforeCpu = Median ($before | ForEach-Object cpuPercentOneCore)
    $afterCpu = Median ($after | ForEach-Object cpuPercentOneCore)
    $beforeCycles = Median ($before | ForEach-Object workerCyclesPerClick)
    $afterCycles = Median ($after | ForEach-Object workerCyclesPerClick)
    if ($beforeCycles -le 0 -or $afterCycles -le 0) { throw 'Worker cycle counter unavailable.' }
    [pscustomobject]@{
      configuredCps=$cps; baselineCpuPercentOneCore=$beforeCpu; candidateCpuPercentOneCore=$afterCpu
      cpuReductionPercent=$(if ($beforeCpu -gt 0) { 100 * ($beforeCpu-$afterCpu)/$beforeCpu } else { $null })
      baselineWorkerCyclesPerClick=$beforeCycles; candidateWorkerCyclesPerClick=$afterCycles
      workerCycleReductionPercent=(100 * ($beforeCycles-$afterCycles)/$beforeCycles)
      baselineSubmissionRate=(Median ($before | ForEach-Object submissionRate))
      candidateSubmissionRate=(Median ($after | ForEach-Object submissionRate))
      candidateMaxStopMs=($after | Measure-Object stopMs -Maximum).Maximum
      candidateFailures=($after | Measure-Object failures -Sum).Sum
    }
  }
  @{
    baselineCommit=$Baseline; candidateCommit=(& git rev-parse HEAD)
    os=([Environment]::OSVersion.VersionString); secondsPerRate=$Seconds; repeats=$Repeats
    baselineStoppedCpuPercentOneCore=(Median ($results.baseline | ForEach-Object stoppedCpuPercentOneCore))
    candidateStoppedCpuPercentOneCore=(Median ($results.candidate | ForEach-Object stoppedCpuPercentOneCore))
    samples=@($comparison)
    note='Native runtime/local receiver only. Process CPU time has coarse Windows accounting granularity. Worker cycles isolate scheduling work but cannot be converted to time across CPU frequencies. Hosted timings do not validate Roblox frame times or physical Windows 10/11 performance.'
  } | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $outputPath 'comparison.json')
  $comparison | Format-Table
  if (@($comparison | Where-Object { $_.candidateFailures -gt 0 -or $_.candidateMaxStopMs -ge 50 }).Count) {
    throw 'Candidate had input failures or stop latency of at least 50 ms.'
  }
  if (@($comparison | Where-Object { $_.configuredCps -ge 500 -and $_.candidateWorkerCyclesPerClick -gt $_.baselineWorkerCyclesPerClick * 1.05 }).Count) {
    throw 'High-rate worker cycle cost regressed by more than 5%.'
  }
  # Rate targets on a shared runner are recorded in raw JSON, not silently
  # promoted to physical-PC acceptance. Explicit failures above remain fatal.
  $global:LASTEXITCODE = 0
} finally { Pop-Location }
