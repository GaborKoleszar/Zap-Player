# Builds the release and packages it as build/installer/ZapSetup-<version>.exe.
# Needs Inno Setup 6 (https://jrsoftware.org/isinfo.php, or: winget install JRSoftware.InnoSetup).
param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

$iscc = (Get-Command iscc -ErrorAction Ignore).Source
if (-not $iscc) {
    $iscc = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) { throw 'Inno Setup 6 was not found. Install it with: winget install JRSoftware.InnoSetup' }

if (-not $SkipBuild) {
    Push-Location $root
    try {
        cmake --preset release | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
        cmake --build --preset release | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    } finally { Pop-Location }
}
foreach ($f in 'Zap.exe', 'libmpv-2.dll') {
    if (-not (Test-Path (Join-Path $root "build\release\dist\$f"))) { throw "Missing build output: $f" }
}

& $iscc /Qp (Join-Path $root 'installer\zap.iss')
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup failed.' }
Get-ChildItem (Join-Path $root 'build\installer') -Filter 'ZapSetup-*.exe' |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1 |
    ForEach-Object { Write-Host ("Installer: {0} ({1:N1} MB)" -f $_.FullName, ($_.Length / 1MB)) }
