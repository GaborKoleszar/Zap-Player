# Measures start-up: milliseconds from process creation to the first video frame on screen.
# Runs the release build several times and fails if the median is over the limit.
#
# About the limit: creating the first Direct3D device costs roughly 200 ms on a typical
# discrete GPU (driver start-up) and nothing in this app can shorten that. The limit guards
# against regressions in everything else. A per-phase timeline of the last runs is written
# to zap-trace.txt in the temp folder.
param(
    [string]$File = (Join-Path $PSScriptRoot 'media\sample.mkv'),
    [int]$Runs = 10,
    [int]$LimitMs = 400,
    [string]$Exe = (Join-Path $PSScriptRoot '..\build\release\dist\Zap.exe')
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path $Exe)) { throw "Build the release first: $Exe not found." }
if (-not (Test-Path $File)) { throw "Sample not found: $File (run bench/make-sample.ps1)." }

$result = Join-Path $env:TEMP 'zap-bench.txt'

function Invoke-Run {
    $p = Start-Process -FilePath $Exe -ArgumentList '--bench', "`"$File`"" -PassThru
    if (-not $p.WaitForExit(15000)) { $p.Kill(); throw 'The player did not report a first frame within 15 s.' }
}

Invoke-Run   # warm-up, not counted
Remove-Item $result -ErrorAction Ignore
for ($i = 0; $i -lt $Runs; $i++) { Invoke-Run }

$values = @(Get-Content $result | ForEach-Object { [int]$_ } | Sort-Object)
if ($values.Count -ne $Runs) { throw "Expected $Runs results, got $($values.Count)." }
$median = if ($Runs % 2) { $values[($Runs - 1) / 2] } else { [int](($values[$Runs / 2 - 1] + $values[$Runs / 2]) / 2) }
Write-Host "runs (ms): $($values -join ', ')"
Write-Host "median: $median ms (limit $LimitMs ms)"
if ($median -gt $LimitMs) { Write-Host 'FAIL: over the limit'; exit 1 }
Write-Host 'OK'
