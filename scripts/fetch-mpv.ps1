# Downloads the prebuilt libmpv dev package into third_party/mpv and creates an MSVC import library.
# The release is pinned so every build gets the same libmpv; pass -Tag to try a newer one
# (the player needs mpv 0.38 or later).
# The DLL is what ends up in the installer, so its SHA-256 is pinned too: a release asset that
# was swapped after the fact stops the build instead of shipping. With another -Tag, pass the
# matching -Sha256 (the error message prints the hash that was found).
param(
    [string]$Tag = '20261111',
    [string]$Sha256 = '88504ee5560942a955405e99dfb5971368e4cf851321f36565bf2a667271db1b'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $root 'third_party\mpv'
function Assert-Hash {
    $actual = (Get-FileHash (Join-Path $dest 'libmpv-2.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Sha256.ToLowerInvariant()) {
        throw "libmpv-2.dll does not match the pinned SHA-256.`n  expected $Sha256`n  found    $actual`nDelete third_party\mpv and run again; if you chose another -Tag, pass its hash with -Sha256."
    }
}
$have = Test-Path (Join-Path $dest "mpv.lib")
if ($have) { Assert-Hash; Write-Host "libmpv already present."; exit 0 }
$haveDll = Test-Path (Join-Path $dest "libmpv-2.dll")
New-Item -ItemType Directory -Force $dest | Out-Null

if (-not $haveDll) {
    $release = Invoke-RestMethod "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/tags/$Tag" -Headers @{ 'User-Agent' = 'fetch-mpv' }
    $asset = $release.assets | Where-Object { $_.name -match '^mpv-dev-x86_64-\d' } | Select-Object -First 1
    if (-not $asset) { throw "No mpv-dev-x86_64 asset found in release $Tag." }
    $archive = Join-Path $dest $asset.name
    Write-Host "Downloading $($asset.name) ..."
    Invoke-WebRequest $asset.browser_download_url -OutFile $archive -Headers @{ 'User-Agent' = 'fetch-mpv' }

    $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
    & $tar -xf $archive -C $dest
    if ($LASTEXITCODE -ne 0) {
        $7z = Get-Command 7z -ErrorAction Ignore
        if (-not $7z) { throw 'Could not extract the .7z archive: Windows tar failed and 7z is not installed.' }
        & $7z.Source x $archive "-o$dest" -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw '7z extraction failed.' }
    }
    Remove-Item $archive
}

foreach ($f in 'libmpv-2.dll', 'include\mpv\client.h') {
    if (-not (Test-Path (Join-Path $dest $f))) { throw "Unexpected package layout: $f is missing." }
}
Assert-Hash

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ build tools not found.' }
$lib = Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Recurse -Filter lib.exe |
    Where-Object { $_.FullName -match 'Hostx64\\x64' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $lib) { throw 'lib.exe not found.' }
if (-not (Test-Path "$dest\mpv.def")) {
    # The package ships no .def file: build one from the DLL's export table.
    $dumpbin = Join-Path $lib.DirectoryName 'dumpbin.exe'
    $names = & $dumpbin /nologo /exports "$dest\libmpv-2.dll" |
        ForEach-Object { if ($_ -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(mpv_\w+)') { $Matches[1] } }
    if (-not $names) { throw 'Could not read exports from libmpv-2.dll.' }
    Set-Content "$dest\mpv.def" (@('EXPORTS') + $names)
}
& $lib.FullName /nologo "/def:$dest\mpv.def" /name:libmpv-2.dll "/out:$dest\mpv.lib" /machine:x64
if ($LASTEXITCODE -ne 0) { throw 'Creating mpv.lib failed.' }
Write-Host "libmpv ready in $dest"
