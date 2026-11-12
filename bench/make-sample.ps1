# Builds the sample media used by the tests, the benchmark and the manual checklist. Needs ffmpeg on PATH.
$ErrorActionPreference = 'Stop'
$media = Join-Path $PSScriptRoot 'media'
New-Item -ItemType Directory -Force $media | Out-Null
if (-not (Get-Command ffmpeg -ErrorAction Ignore)) { throw 'ffmpeg was not found on PATH.' }

function Invoke-FFmpeg {
    & ffmpeg -hide_banner -loglevel error -y @args
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed: $args" }
}

$embedded = Join-Path $media 'embedded.tmp.srt'
Set-Content $embedded "1`n00:00:00,500 --> 00:00:04,000`nEmbedded subtitle line`n`n2`n00:00:05,000 --> 00:00:09,000`nSecond embedded line`n"
Set-Content (Join-Path $media 'sample.srt') "1`n00:00:00,500 --> 00:00:04,000`nSubtitle from the file next to the video`n`n2`n00:00:05,000 --> 00:00:09,000`nSecond line from the file`n"

Invoke-FFmpeg -f lavfi -i 'testsrc2=size=1920x1080:rate=30:duration=10' `
    -f lavfi -i 'sine=frequency=440:duration=10' -f lavfi -i 'sine=frequency=880:duration=10' -i $embedded `
    -map '0:v' -map '1:a' -map '2:a' -map '3:s' '-c:v' libx264 -preset veryfast -pix_fmt yuv420p '-c:a' aac '-c:s' srt `
    '-metadata:s:a:0' language=eng '-metadata:s:a:1' language=hun '-metadata:s:s:0' language=eng `
    (Join-Path $media 'sample.mkv')
Remove-Item $embedded

Invoke-FFmpeg -f lavfi -i 'sine=frequency=440:duration=5' '-c:a' aac (Join-Path $media 'audio-only.m4a')
Invoke-FFmpeg -f lavfi -i 'testsrc2=size=1280x720:rate=30:duration=5' '-c:v' libx264 -preset veryfast -pix_fmt yuv420p -an (Join-Path $media 'no-audio.mp4')

# A longer clip for checking resume (positions are only remembered between 5 s and 10 s before the end).
Invoke-FFmpeg -f lavfi -i 'testsrc2=size=640x360:rate=30:duration=60' -f lavfi -i 'sine=frequency=330:duration=60' '-c:v' libx264 -preset veryfast -pix_fmt yuv420p '-c:a' aac (Join-Path $media 'long.mp4')

$bytes = New-Object byte[] 200
[System.Random]::new(42).NextBytes($bytes)
[System.IO.File]::WriteAllBytes((Join-Path $media 'broken.mkv'), $bytes)

Copy-Item (Join-Path $media 'sample.mkv') (Join-Path $media 'Árvíztűrő tükörfúrógép.mkv') -Force
Write-Host "Sample media written to $media"
