<p align="center"><img src="docs/zap-icon.png" width="96" alt="Zap icon"></p>

# Zap

A small, fast video player for Windows 11 (x64). Double-click a video and it plays almost at once.

## Why

Zap exists because of one annoyance: on a fast PC, double-clicking a video should not mean
waiting a few seconds for the player to start. Most of the time you just want to watch the
file, right now.

So Zap has one goal above the others: get the first frame on screen as fast as possible. On the
development PC that takes about 350 ms from the double-click, most of which is Windows and the
GPU driver starting up. The start-up time is measured by a benchmark in this repository, and
anything not needed for the first frame is left until after it.

Everything else follows from keeping the video in focus:

- **Plays most things.** Playback is done by [libmpv](https://mpv.io), with hardware decoding
  where the GPU supports it.
- **Looks like Windows 11.** A thin native Win32 window, no skin and no web view. It follows
  dark and light mode and your accent colour. The title bar, the docked control bar and the
  empty window share the Mica backdrop; the floating control panel is frosted glass over the
  video.
- **Stays out of the way.** The controls fade while the video plays, and a borderless mode
  leaves nothing but the picture.
- **The basics, done:** subtitles, audio tracks, playback speed, fullscreen, and resuming
  each video where you stopped.

What it deliberately leaves out: playlists, a media library, streaming from URLs, discs, and a
settings screen.

## Install

Download `ZapSetup-<version>.exe` from the Releases page and run it. It installs for the
current user only, without an administrator prompt, and needs Windows 11. The installer is not
code-signed, so Windows SmartScreen may ask for confirmation before it runs.

To make Zap the default player, right-click a video, choose Open with > Choose another app,
pick Zap and "Always". Windows does not let an installer do this for you.

## Use

- Open a file: pass it on the command line, drop it on the window, press Ctrl+O, or set
  Zap as the "Open with" app for a file type in Explorer (right-click a video >
  Open with > Choose another app > pick `Zap.exe`).
- Opening another video while one is playing replaces it in the same window.
- The player remembers where you stopped in each video and resumes there.
- A `.srt` / `.ass` file next to the video with the same name is loaded automatically.
- Right-click the video for the menu.
- Volume, control layout, borderless mode and the window's size and position are remembered
  between runs. Menu > Settings > "Always start with defaults" turns that off: every start then
  begins at full volume with floating controls in a standard window.

| Key | Action |
| --- | --- |
| Space | Play / pause |
| Left / Right | Seek 5 s (60 s with Ctrl) |
| Up / Down, mouse wheel | Volume |
| M | Mute |
| F, F11, double-click | Fullscreen (Esc leaves it) |
| B | Borderless window |
| L | Switch controls between floating and docked |
| S / A | Next subtitle / audio track |
| `[` / `]`, numpad - / + | Slower / faster |
| Ctrl+O | Open file |

Drag the video to move the window. In borderless mode, drag the window edges to resize.

Settings and resume positions are stored in `%APPDATA%\Zap\`. Compiled shaders are cached
in `%LOCALAPPDATA%\Zap\cache\` (safe to delete; the next start is then slower once).

## Build

Needs Visual Studio Build Tools 2022 (C++), CMake, and PowerShell 7.

```
pwsh scripts/fetch-mpv.ps1          # once: downloads a pinned libmpv release into third_party/mpv
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
```

The player ends up in `build/release/dist/`: `Zap.exe` and `libmpv-2.dll`. Those two files
are the whole app; copy the folder anywhere (when passing it on, include `LICENSE` and
`THIRD-PARTY-NOTICES.txt`).

The unit tests that play real files need sample media (requires `ffmpeg` on PATH):

```
pwsh bench/make-sample.ps1
```

`./build.sh` (Git Bash) does configure + build + tests in one go.

## Building the installer

```
pwsh scripts/build-installer.ps1
```

builds `build/installer/ZapSetup-<version>.exe`. It needs Inno Setup 6
(`winget install JRSoftware.InnoSetup`). The version comes from `project(Zap VERSION ...)` in
`CMakeLists.txt`.

The installer works per user, without an administrator prompt. It puts Zap in
`%LOCALAPPDATA%\Programs\Zap`, adds a Start menu entry and an uninstaller, and registers
Zap as a player for common video types, so Windows suggests it under "Open with" and in
Settings > Default apps. Windows does not let an installer change the default
player: right-click a video, choose Open with > Choose another app, pick Zap and "Always".
Uninstalling removes the program, the shortcuts, the registry entries and the shader cache; it
keeps your settings and resume positions in `%APPDATA%\Zap`.

## Start-up time

```
pwsh bench/run.ps1
```

launches the release build ten times and prints the median time from process start to the first
video frame on screen. It fails if the median is over 400 ms. For a per-phase timeline, look at
`zap-trace.txt` in the temp folder afterwards.

On the development PC (RTX 4070 Super) the median is about 350 ms. Roughly 215 ms of that is
Windows and the GPU driver creating the first Direct3D device, which any hardware-accelerated
player pays and which this app cannot shorten.

To see what mpv is doing, set the environment variable `VP_MPV_LOG` to a file path before starting
the player; mpv writes its verbose log there.

## Layout

- `core/` — portable C++ (no Windows headers): libmpv wrapper, settings, resume positions,
  control bar layout. Meant to be reused by a future macOS shell.
- `win/` — the Windows shell: window, control bar drawing, floating panel, menu.
- `tests/` — unit tests for `core/`.
- `bench/` — sample media and the start-up benchmark.
- `scripts/make-icon.py` — redraws `win/zap.ico` (needs Pillow).
- `docs/manual-checklist.md` — what to check by hand before calling a build good.

## Licence

Zap is free software under the GNU General Public License, version 3 or later; see
[LICENSE](LICENSE).

The installer also contains a prebuilt GPL build of libmpv. Where it comes from and where its
source is: [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt). Both files are installed next
to `Zap.exe`. `scripts/fetch-mpv.ps1` checks the downloaded `libmpv-2.dll` against a pinned
SHA-256, so a build always ships the library named there.
