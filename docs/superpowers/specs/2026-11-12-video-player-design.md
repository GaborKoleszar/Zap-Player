# Video Player — Design

Date: 2026-11-12
Status: awaiting approval

## Intent

A small native video player for Windows 11 x64 (macOS later) whose point is
speed: double-click a video file and the first frame is on screen almost
instantly. The motivating annoyance is VLC taking a few seconds to start
playing on a fast PC.

The video is the focus. Controls are minimal, look like they belong on
Windows 11, and stay out of the way.

## Goals

1. **Fast startup.** For a local file, first frame on screen within about
   300 ms of the double-click (warm start, on the owner's PC). This is
   measured on every build so it never creeps up.
2. **Plays most things.** Broad container and codec support, comparable to
   VLC, with hardware-accelerated decoding.
3. **Native, minimal interface** that follows Windows dark/light mode and
   accent color.
4. **Day-one features:** subtitles, audio track switching, playback speed,
   fullscreen, borderless toggle, resume where you left off.

## Non-goals (not on day one)

- Playlists, media library
- Streaming from URLs
- DVD / Blu-ray discs
- A settings screen
- An installer or automatic file-association registration (the owner sets
  "Open with" by hand)
- The macOS app itself (only kept possible, see Structure)
- Languages other than English

## Anti-goals

- Anything that costs startup time for a feature that isn't needed to show
  the first frame. When in doubt, defer the work until after the first frame.
- A cross-platform UI toolkit that looks the same everywhere. Each platform
  gets its own native shell.
- Controls that look like a web page or a custom "skin".

## Behavior (decided with the owner)

### Window and controls

- The video fills the window.
- Two control layouts, switchable, choice remembered:
  - **Floating** (default): a translucent rounded panel hovering over the
    bottom of the video, in the style of Windows 11's own media controls.
    Fades out while playing; reappears on mouse move.
  - **Docked**: a slim bar under the video that never covers the picture;
    always visible in a normal window. In fullscreen and borderless it
    behaves like the floating one (hidden until the mouse moves).
- Switch layouts from the right-click menu and a keyboard shortcut.
- Bar contents, left to right: play/pause, time, seek bar, volume slider,
  subtitles, audio track, speed, borderless, fullscreen.
- Right-click menu on the video offers everything the bar has (subtitle
  track, audio track, speed, borderless, fullscreen, layout switch, open
  file), so it all works with the controls hidden.
- **Borderless:** the window becomes only the video, with no title bar or
  frame. It can still be dragged and resized.
- Double-click the video toggles fullscreen. Dragging the video moves the
  window. Mouse wheel over the video changes volume.
- Usual keyboard shortcuts: Space pause, Left/Right seek, Up/Down volume,
  F fullscreen, Esc leave fullscreen. (Exact full shortcut list: builder's
  call, following mpv/VLC conventions.)
- Follows Windows dark/light mode and accent color.

### Files

- Opening the app with no file shows an empty dark window with a single
  "Open file" button; a file can also be dropped onto the window.
- Opening a second video while one is playing replaces it in the same
  window (single instance).
- Subtitles: tracks inside the file, plus `.srt`/`.ass` files sitting next
  to the video, picked up automatically.
- The player remembers the position in each video and resumes there.

### Smaller behaviors (designer's calls, not discussed with the owner)

- The window opens at its last-used size and position, and the video is
  letterboxed inside it. It does not resize itself to each video, which
  would make the window jump after it appears.
- Volume and the borderless state are remembered between runs; fullscreen
  is not.
- At the end of a video the player stays open, paused on the last frame,
  and that video's resume position is cleared.
- A file that can't be played shows a short message in the window instead
  of a dialog box.
- External subtitle files are matched by the video's file name, the way
  mpv does it by default.

## How it gets made (designer's calls — the owner left these open)

- **Playback engine: libmpv.** Plays essentially everything, has solid
  hardware decoding on Windows (D3D11) and macOS, and starts much faster
  than VLC. Building on raw FFmpeg would mean re-doing sync, subtitles and
  HDR by hand.
- **Windows shell: C++20, plain Win32, built with CMake and MSVC.** Visual
  Studio Build Tools 2022 and CMake are already installed on the owner's PC.
  No WinUI 3, Qt, or web-based toolkit: each adds noticeable startup time.
- **Video embedding:** mpv renders into a child window of the main window
  (its `wid` option) using its own D3D11 output. mpv's render API is
  OpenGL-only, which would give up the best hardware-decode path.
- **Controls drawing:** because a D3D child window can't be drawn over from
  the same window, the floating panel is a separate small window owned by
  the main window and kept positioned over it, using the system acrylic
  backdrop and rounded corners from DWM. Controls are drawn with Direct2D /
  DirectWrite and Segoe Fluent Icons. The docked bar is a normal child
  strip of the main window using the same drawing code.
- **Startup rules:**
  - Create and show the window first, start mpv and `loadfile` immediately,
    and do everything else (settings beyond the essentials, control
    drawing resources, menus) after or in parallel.
  - mpv configured lean: no user config files, no scripts, no built-in
    on-screen controller, no youtube-dl hook, hardware decoding on
    (`hwdec=auto-safe`).
  - Single instance via a named mutex; a second launch hands its file path
    to the running window and exits.
  - No runtime dependencies beyond `libmpv-2.dll` next to the exe; static
    C runtime.
- **Startup measurement:** a built-in benchmark switch that logs
  milliseconds from process creation to the first displayed frame, and a
  script that runs it several times against a sample file and reports the
  median. Target: ≤ 300 ms warm.
- **Resume and settings storage:** a small file under `%APPDATA%`. Whether
  resume uses mpv's own watch-later feature or our own store is the
  builder's call.
- **Distribution:** a folder with the exe and `libmpv-2.dll`. No installer.
- **libmpv binaries:** prebuilt Windows dev builds (GPL). Fine for personal
  use; if this is ever distributed, the app would need to be GPL too or
  switch to an LGPL build of mpv. Flagging, not deciding.

## Structure

Split so the Mac version can reuse everything that isn't Windows-specific:

- `core/` — portable C++: thin wrapper around libmpv (load, play/pause,
  seek, tracks, speed, volume, events), settings, resume positions, time
  formatting, command-line parsing. No Windows headers.
- `win/` — the Windows shell: window, borderless/fullscreen handling,
  single instance, control panel drawing, menus, input.
- `tests/` — unit tests for `core/` pieces that don't need a video
  (settings, resume store, time formatting, command line).
- `bench/` — startup benchmark script.

The Mac shell would later be a sibling of `win/` (Swift/AppKit, designer's
guess) over the same `core/`.

## Testing

- Unit tests for the portable core.
- Startup benchmark as described, run on every build.
- A short manual checklist for the interface: both layouts, borderless,
  fullscreen, track switching, speed, subtitles next to the file, resume,
  second file replaces first, dark/light mode.

## Left to the builder

- Exact shortcut list, speed steps, seek step sizes
- Fade timings and panel dimensions
- Settings file format
- How resume positions are stored
- Which prebuilt libmpv package to use and how it is fetched at build time
- App name and icon (working name: VideoPlayer)

## Housekeeping

- `D:\Documents\Projects\VideoPlayer` currently sits inside a git
  repository rooted at `D:\Documents` whose history belongs to another
  project. This project should get its own repository in this folder
  before any code is committed. Add `.superpowers/` to its `.gitignore`.
