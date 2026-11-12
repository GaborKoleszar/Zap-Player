# Video Player Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A native Windows 11 x64 video player on libmpv that shows the first frame within ~300 ms of a double-click, with minimal native-looking controls.

**Architecture:** A portable C++ `core/` library (libmpv wrapper, settings, resume store, pure layout/format logic, no Windows headers) and a Win32 shell in `win/`. mpv renders into a child window via `wid`; controls are drawn with Direct2D either in a docked strip or in a separate owned acrylic popup window floating over the video.

**Tech Stack:** C++20, CMake (generator "Visual Studio 17 2022", x64), MSVC Build Tools 2022, libmpv (prebuilt dev package), Win32 + DWM + Direct2D/DirectWrite, doctest for unit tests, PowerShell scripts.

**Spec:** `docs/superpowers/specs/2026-11-12-video-player-design.md`

## Global Constraints

- Windows 11 x64 only for now; `core/` must not include any Windows header.
- Startup target: first frame ≤ 300 ms after process creation, warm start, local file. Nothing that isn't needed for the first frame may run before it.
- No WinUI 3, Qt, or web toolkit. No runtime dependency besides `libmpv-2.dll` next to the exe; static C runtime (`/MT`).
- mpv options always set: `config=no`, `load-scripts=no`, `osc=no`, `osd-bar=no`, `ytdl=no`, `input-default-bindings=no`, `input-vo-keyboard=no`, `input-cursor=no`, `hwdec=auto-safe`, `gpu-api=d3d11`, `keep-open=yes`, `idle=yes`.
- Interface text is English only. All paths cross the core boundary as UTF-8 `std::string`; the shell converts to/from UTF-16.
- User data lives in `%APPDATA%\VideoPlayer\` (`settings.ini`, `resume.ini`).
- Distribution is a folder: `VideoPlayer.exe` + `libmpv-2.dll`. No installer, no file-association registration.
- The project gets its own git repository in `D:\Documents\Projects\VideoPlayer` (the folder currently sits inside an unrelated repo rooted at `D:\Documents`). `.superpowers/`, `build/`, `third_party/mpv/` are git-ignored.

## Review Focus

1. **Non-ASCII and long file paths** (e.g. `Árvíztűrő tükörfúrógép.mkv`, paths > 260 chars): must open, resume, and hand over to a running instance correctly. Tests in Tasks 2, 4, 8.
2. **Saved window position is off-screen** (monitor unplugged, resolution changed): the window must reopen fully on a visible monitor. Test in Task 3.
3. **Corrupt or half-written settings/resume files:** must fall back to defaults, never crash or refuse to start. Tests in Tasks 3, 4.
4. **Files without a video or without an audio track, and files that fail to load:** audio-only shows the empty dark window with working controls; no-audio hides nothing and doesn't error; unplayable shows the in-window message. Tests in Task 5; manual checks in Task 13.
5. **Second launch while the first window is fullscreen, minimized, or mid-load:** the new file replaces the old one and the window comes to the foreground, state intact. Manual check in Task 8.

---

## File Structure

```
CMakeLists.txt                 top-level: options, /MT, subdirs
CMakePresets.json              presets "release" and "debug" (VS 2022, x64, build/ dir)
.gitignore
scripts/fetch-mpv.ps1          downloads + unpacks libmpv dev package into third_party/mpv, makes mpv.lib
core/CMakeLists.txt
core/include/vp/time_format.h      format_time
core/include/vp/command_line.h     CommandLine, parse_command_line
core/include/vp/geometry.h         Rect, Size, fit_rect_to_work_areas
core/include/vp/settings.h         Settings, load_settings, save_settings
core/include/vp/resume_store.h     ResumeStore, resume_key, should_resume
core/include/vp/bar_layout.h       BarElement, BarLayout, compute_bar_layout, hit_test
core/include/vp/player.h           Player, PlayerState, Track, PlayerEvent
core/src/*.cpp                     one .cpp per header
tests/CMakeLists.txt               doctest via FetchContent, target vp_tests
tests/test_*.cpp                   one per core header
win/CMakeLists.txt                 target VideoPlayer (WIN32 exe)
win/main.cpp                       wWinMain: single instance, window first, then mpv
win/app_window.{h,cpp}             main window, mpv child, placement, fullscreen/borderless, input
win/single_instance.{h,cpp}        mutex + WM_COPYDATA hand-over
win/control_bar.{h,cpp}            Direct2D drawing + interaction of the bar (shared by both layouts)
win/floating_panel.{h,cpp}         owned acrylic popup hosting a ControlBar, fade/auto-hide
win/context_menu.{h,cpp}           right-click menu and track/speed submenus
win/theme.{h,cpp}                  dark/light + accent colour, change notifications
win/paths.{h,cpp}                  %APPDATA% dir, UTF-8/UTF-16 conversion
win/bench.{h,cpp}                  --bench timing and result file
bench/make-sample.ps1              builds sample media with ffmpeg
bench/run.ps1                      runs --bench N times, prints median, fails over 300 ms
docs/manual-checklist.md
```

---

### Task 1: Repository, build skeleton, libmpv fetch

**Files:**
- Create: `.gitignore`, `CMakeLists.txt`, `CMakePresets.json`, `scripts/fetch-mpv.ps1`, `core/CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/test_smoke.cpp`, `win/CMakeLists.txt`, `win/main.cpp`

**Interfaces:**
- Produces: CMake targets `vp_core` (static lib, include dir `core/include`), `vp_tests` (doctest exe registered with CTest), `VideoPlayer` (WIN32 exe); imported target `mpv::mpv` (headers in `third_party/mpv/include`, `mpv.lib`, and a post-build copy of `libmpv-2.dll` next to `VideoPlayer.exe` and `vp_tests.exe`).
- Build commands used by every later task: `cmake --preset release`, `cmake --build --preset release`, `ctest --preset release --output-on-failure`.

- [ ] **Step 1:** `git init` in the project folder; write `.gitignore` with `build/`, `third_party/mpv/`, `.superpowers/`, `bench/media/`. Commit the existing `docs/` folder.
- [ ] **Step 2:** Write `scripts/fetch-mpv.ps1`. It downloads the latest `mpv-dev-x86_64-*.7z` asset (not the `-v3` variant) from the GitHub releases of `shinchiro/mpv-winbuild-cmake`, extracts it to `third_party/mpv/` (try Windows `tar -xf`; fall back to `7z` if present; fail with a clear message otherwise), and generates an MSVC import library by running `lib /def:mpv.def /name:libmpv-2.dll /out:mpv.lib /machine:x64` from a VS developer environment (locate with `vswhere`). It is a no-op if `third_party/mpv/mpv.lib` exists. Verify the package layout after the first download and adjust paths if the archive differs (expected: `libmpv-2.dll`, `mpv.def`, `include/mpv/client.h`).
- [ ] **Step 3:** Run `pwsh scripts/fetch-mpv.ps1`. Expected: `third_party/mpv/mpv.lib`, `libmpv-2.dll`, `include/mpv/client.h` exist.
- [ ] **Step 4:** Write the CMake files. Top level sets C++20, `CMAKE_MSVC_RUNTIME_LIBRARY` to `MultiThreaded$<$<CONFIG:Debug>:Debug>`, `/W4 /utf-8`, defines `UNICODE`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`. `vp_core` starts with one placeholder source. `tests/` fetches doctest v2.4.11 with FetchContent.
- [ ] **Step 5:** `tests/test_smoke.cpp`: one test `TEST_CASE("mpv client api is linkable")` asserting `mpv_client_api_version() >= MPV_MAKE_VERSION(2, 0)`.
- [ ] **Step 6:** `win/main.cpp`: `wWinMain` that registers a window class, creates and shows an 1280×720 window titled "VideoPlayer" with a black background, and runs a message loop.
- [ ] **Step 7:** Run the three build commands. Expected: build succeeds, 1 test passes, `build/release/win/Release/VideoPlayer.exe` (or the preset's equivalent path; record the real path in the README stub) opens a black window.
- [ ] **Step 8:** Commit: `chore: project skeleton with libmpv and tests`.

### Task 2: Time formatting and command line

**Files:**
- Create: `core/include/vp/time_format.h`, `core/src/time_format.cpp`, `core/include/vp/command_line.h`, `core/src/command_line.cpp`
- Test: `tests/test_time_format.cpp`, `tests/test_command_line.cpp`

**Interfaces:**
- Produces:
  - `std::string vp::format_time(double seconds);`
  - `struct vp::CommandLine { std::optional<std::string> file_utf8; bool bench = false; };`
  - `vp::CommandLine vp::parse_command_line(const std::vector<std::string>& args_utf8);` (args exclude the program name)

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("format_time") {
    CHECK(vp::format_time(7) == "0:07");
    CHECK(vp::format_time(1450) == "24:10");
    CHECK(vp::format_time(3822) == "1:03:42");
    CHECK(vp::format_time(59.9) == "0:59");
    CHECK(vp::format_time(-3) == "0:00");
    CHECK(vp::format_time(std::nan("")) == "0:00");
}
TEST_CASE("parse_command_line") {
    CHECK_FALSE(vp::parse_command_line({}).file_utf8.has_value());
    auto a = vp::parse_command_line({"C:\\v\\a b.mkv"});
    CHECK(a.file_utf8 == "C:\\v\\a b.mkv");
    CHECK_FALSE(a.bench);
    auto b = vp::parse_command_line({"--bench", "x.mp4"});
    CHECK(b.bench);
    CHECK(b.file_utf8 == "x.mp4");
    auto name = u8_as_string(u8"D:\\Árvíztűrő tükörfúrógép.mkv");   // helper in tests/test_util.h: char8_t -> std::string
    CHECK(vp::parse_command_line({name}).file_utf8 == name);         // bytes pass through untouched
    CHECK(vp::parse_command_line({"a.mkv", "b.mkv"}).file_utf8 == "a.mkv"); // first file wins
    CHECK_FALSE(vp::parse_command_line({"--unknown"}).file_utf8.has_value()); // unknown switches ignored
}
```

- [ ] **Step 2:** Build and run `ctest`. Expected: compile failure (headers missing).
- [ ] **Step 3:** Implement both functions. Arguments starting with `--` are switches; anything else is a file.
- [ ] **Step 4:** Run `ctest`. Expected: PASS.
- [ ] **Step 5:** Commit: `feat(core): time formatting and command line parsing`.

### Task 3: Geometry and settings

**Files:**
- Create: `core/include/vp/geometry.h`, `core/src/geometry.cpp`, `core/include/vp/settings.h`, `core/src/settings.cpp`
- Test: `tests/test_geometry.cpp`, `tests/test_settings.cpp`

**Interfaces:**
- Produces:
  - `struct vp::Rect { int x, y, w, h; };  struct vp::Size { int w, h; };`
  - `vp::Rect vp::fit_rect_to_work_areas(std::optional<Rect> saved, std::span<const Rect> work_areas, Size fallback);` — returns `saved` if at least 100×100 px of it lies inside one work area; otherwise `fallback` size centred in `work_areas[0]`, shrunk to fit it.
  - `enum class vp::ControlLayout { Floating, Docked };`
  - `struct vp::Settings { ControlLayout layout = ControlLayout::Floating; int volume = 100; bool borderless = false; std::optional<Rect> window; };`
  - `vp::Settings vp::load_settings(const std::filesystem::path& file);`
  - `bool vp::save_settings(const std::filesystem::path& file, const Settings& s);` — writes to `file + ".tmp"` then renames; creates the parent directory.
- File format: `key=value` lines: `layout=floating|docked`, `volume=0..100`, `borderless=0|1`, `window=x,y,w,h`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("fit_rect keeps a visible saved rect") {
    vp::Rect wa[] = {{0, 0, 1920, 1040}};
    auto r = vp::fit_rect_to_work_areas(vp::Rect{100, 100, 800, 600}, wa, {1280, 720});
    CHECK(r.x == 100); CHECK(r.w == 800);
}
TEST_CASE("fit_rect recentres an off-screen rect") {
    vp::Rect wa[] = {{0, 0, 1920, 1040}};
    auto r = vp::fit_rect_to_work_areas(vp::Rect{5000, 100, 800, 600}, wa, {1280, 720});
    CHECK(r.w == 1280); CHECK(r.h == 720); CHECK(r.x == 320); CHECK(r.y == 160);
}
TEST_CASE("fit_rect shrinks fallback on a small screen and handles no saved rect") {
    vp::Rect wa[] = {{0, 0, 1024, 600}};
    auto r = vp::fit_rect_to_work_areas(std::nullopt, wa, {1280, 720});
    CHECK(r.w <= 1024); CHECK(r.h <= 600); CHECK(r.x >= 0); CHECK(r.y >= 0);
}
TEST_CASE("settings round trip") {
    auto f = temp_dir() / "s.ini";
    vp::Settings s; s.layout = vp::ControlLayout::Docked; s.volume = 35; s.borderless = true; s.window = vp::Rect{10, 20, 900, 500};
    REQUIRE(vp::save_settings(f, s));
    auto t = vp::load_settings(f);
    CHECK(t.layout == vp::ControlLayout::Docked); CHECK(t.volume == 35); CHECK(t.borderless); CHECK(t.window->w == 900);
}
TEST_CASE("settings: missing or corrupt file gives defaults, bad values are clamped or ignored") {
    CHECK(vp::load_settings(temp_dir() / "nope.ini").volume == 100);
    write_file(temp_dir() / "bad.ini", "volume=900\nlayout=sideways\nwindow=1,2\n\x00\xff garbage\nborderless=1\n");
    auto t = vp::load_settings(temp_dir() / "bad.ini");
    CHECK(t.volume == 100); CHECK(t.layout == vp::ControlLayout::Floating); CHECK_FALSE(t.window.has_value()); CHECK(t.borderless);
}
```

(`temp_dir()` and `write_file()` are small helpers in `tests/test_util.h`: a fresh unique directory under the system temp dir.)

- [ ] **Step 2:** Run `ctest`. Expected: compile failure.
- [ ] **Step 3:** Implement. Out-of-range `volume` clamps to 0–100; unknown keys and malformed lines are skipped.
- [ ] **Step 4:** Run `ctest`. Expected: PASS.
- [ ] **Step 5:** Commit: `feat(core): settings and window placement fitting`.

### Task 4: Resume store

**Files:**
- Create: `core/include/vp/resume_store.h`, `core/src/resume_store.cpp`
- Test: `tests/test_resume_store.cpp`

**Interfaces:**
- Produces:
  - `std::string vp::resume_key(const std::string& path_utf8);` — absolute, lexically normalised path with `\` separators, lower-cased (Unicode-aware lower-casing is not required: lower-case ASCII only, leave other bytes unchanged).
  - `bool vp::should_resume(double position, double duration);` — true when `position >= 5` and `duration - position >= 10`.
  - `class vp::ResumeStore { public: explicit ResumeStore(std::filesystem::path file); std::optional<double> get(const std::string& key) const; void set(const std::string& key, double seconds); void clear(const std::string& key); bool save() const; };`
- The store keeps at most 500 entries; `set` on a full store drops the least recently set entry. File format: one `seconds<TAB>key` line per entry, oldest first; written via temp file + rename.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("should_resume") {
    CHECK_FALSE(vp::should_resume(3, 600));
    CHECK(vp::should_resume(120, 600));
    CHECK_FALSE(vp::should_resume(595, 600));
    CHECK_FALSE(vp::should_resume(120, 0)); // unknown duration
}
TEST_CASE("resume_key is stable across spelling of the same path") {
    CHECK(vp::resume_key("C:/Videos/Sub/../A.MKV") == vp::resume_key("c:\\videos\\a.mkv"));
    CHECK(vp::resume_key(u8_as_string(u8"D:\\Árvíztűrő.mkv")) == vp::resume_key(u8_as_string(u8"d:/Árvíztűrő.mkv")));
}
TEST_CASE("store round trip, clear, and persistence") {
    auto f = temp_dir() / "r.ini";
    { vp::ResumeStore s(f); s.set("a", 12.5); s.set("b", 99); s.clear("b"); REQUIRE(s.save()); }
    vp::ResumeStore s(f);
    CHECK(s.get("a") == doctest::Approx(12.5)); CHECK_FALSE(s.get("b").has_value());
}
TEST_CASE("store caps at 500 entries, dropping the oldest") {
    vp::ResumeStore s(temp_dir() / "cap.ini");
    for (int i = 0; i < 501; ++i) s.set("k" + std::to_string(i), i + 1.0);
    CHECK_FALSE(s.get("k0").has_value()); CHECK(s.get("k500").has_value());
}
TEST_CASE("corrupt store file loads as empty or partial, never throws") {
    write_file(temp_dir() / "bad.ini", "abc\tx\n12\n\t\n30\tgood\n");
    vp::ResumeStore s(temp_dir() / "bad.ini");
    CHECK(s.get("good") == doctest::Approx(30));
}
```

- [ ] **Step 2:** Run `ctest`. Expected: compile failure.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run `ctest`. Expected: PASS.
- [ ] **Step 5:** Commit: `feat(core): resume position store`.

### Task 5: Player (libmpv wrapper) and sample media

**Files:**
- Create: `core/include/vp/player.h`, `core/src/player.cpp`, `bench/make-sample.ps1`
- Test: `tests/test_player.cpp`

**Interfaces:**
- Produces:

```cpp
namespace vp {
enum class TrackType { Video, Audio, Subtitle };
struct Track { int64_t id; TrackType type; std::string title; std::string lang; bool selected; bool external; };
struct PlayerState { bool has_file = false; bool paused = false; double position = 0; double duration = 0; int volume = 100; bool muted = false; double speed = 1.0; bool has_video = false; };
enum class PlayerEvent { FileLoaded, FirstFrame, EndOfFile, LoadFailed, StateChanged, TracksChanged };
struct PlayerOptions { int64_t wid = 0; int volume = 100; };   // wid 0 = no video output (tests)
class Player {
public:
    explicit Player(const PlayerOptions&);
    ~Player();
    void load(const std::string& path_utf8, double start_seconds = 0);
    void set_pause(bool); void toggle_pause();
    void seek_relative(double seconds); void seek_absolute(double seconds);
    void set_volume(int percent); void toggle_mute();
    void set_speed(double);
    std::vector<Track> tracks() const;
    void select_audio(int64_t id);
    void select_subtitle(int64_t id);          // 0 turns subtitles off
    const PlayerState& state() const;          // updated by poll()
    void set_wakeup(std::function<void()>);    // called from an mpv thread when poll() has work
    std::vector<PlayerEvent> poll();           // drain mpv events; call on the owning thread
};
}
```

- Behaviour: the constructor applies the Global Constraints options before `mpv_initialize`; with `wid == 0` it also sets `vo=null`. State is kept current by observing `pause`, `time-pos`, `duration`, `volume`, `mute`, `speed`, `eof-reached`, `track-list`. `FirstFrame` is emitted on the first `MPV_EVENT_PLAYBACK_RESTART` after each load; `EndOfFile` when `eof-reached` becomes true; `LoadFailed` on `MPV_EVENT_END_FILE` with reason error. `load` with `start_seconds > 0` passes it as the `start` per-file option.
- `bench/make-sample.ps1` uses `ffmpeg` (must be on PATH) to write into `bench/media/`: `sample.mkv` (10 s, 1920×1080 H.264 `testsrc2`, two AAC audio tracks tagged `eng` and `hun`, one embedded SRT subtitle track), `sample.srt` next to it, `audio-only.m4a` (5 s), `no-audio.mp4` (5 s), and `broken.mkv` (200 bytes of random data). Tests locate the folder through a compile definition `VP_MEDIA_DIR` and are skipped with a message when it is empty.

- [ ] **Step 1:** Write and run `bench/make-sample.ps1`. Expected: the five files exist.
- [ ] **Step 2: Write the failing tests** (helper `wait_for(player, PlayerEvent, timeout 5s)` pumps `poll()` until the event arrives):

```cpp
TEST_CASE("loads a file and reports state and tracks") {
    vp::Player p({}); p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    CHECK(p.state().has_file); CHECK(p.state().has_video);
    CHECK(p.state().duration == doctest::Approx(10).epsilon(0.1));
    auto t = p.tracks();
    CHECK(count(t, vp::TrackType::Audio) == 2);
    CHECK(count(t, vp::TrackType::Subtitle) == 2);   // embedded + sample.srt next to the file
    CHECK(any_external(t));
}
TEST_CASE("pause, seek, speed, volume, track selection") {
    vp::Player p({}); p.load(media("sample.mkv")); REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    p.set_pause(true); p.seek_absolute(5); p.set_speed(1.5); p.set_volume(40);
    pump(p, 300ms);
    CHECK(p.state().paused); CHECK(p.state().position == doctest::Approx(5).epsilon(0.2));
    CHECK(p.state().speed == doctest::Approx(1.5)); CHECK(p.state().volume == 40);
    auto second_audio = nth(p.tracks(), vp::TrackType::Audio, 1).id;
    p.select_audio(second_audio); p.select_subtitle(0); pump(p, 300ms);
    CHECK(selected_id(p.tracks(), vp::TrackType::Audio) == second_audio);
    CHECK(selected_id(p.tracks(), vp::TrackType::Subtitle) == 0);
}
TEST_CASE("start position, end of file keeps the file open") {
    vp::Player p({}); p.load(media("sample.mkv"), 8.0);
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame)); CHECK(p.state().position >= 7.5);
    REQUIRE(wait_for(p, vp::PlayerEvent::EndOfFile)); CHECK(p.state().has_file);
}
TEST_CASE("audio-only and no-audio files load; broken file reports LoadFailed") {
    { vp::Player p({}); p.load(media("audio-only.m4a")); REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame)); CHECK_FALSE(p.state().has_video); }
    { vp::Player p({}); p.load(media("no-audio.mp4")); REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame)); CHECK(p.state().has_video); }
    { vp::Player p({}); p.load(media("broken.mkv")); REQUIRE(wait_for(p, vp::PlayerEvent::LoadFailed)); CHECK_FALSE(p.state().has_file); }
}
```

- [ ] **Step 3:** Run `ctest`. Expected: compile failure.
- [ ] **Step 4:** Implement `Player` (pimpl; `mpv_handle` owned by the impl; `mpv_set_wakeup_callback` forwards to the stored function).
- [ ] **Step 5:** Run `ctest`. Expected: PASS.
- [ ] **Step 6:** Commit: `feat(core): libmpv player wrapper`.

### Task 6: Window that plays a file

**Files:**
- Create: `win/paths.{h,cpp}`, `win/app_window.{h,cpp}`
- Modify: `win/main.cpp`

**Interfaces:**
- Consumes: `vp::Player`, `vp::Settings`, `load_settings`/`save_settings`, `fit_rect_to_work_areas`, `ResumeStore`, `resume_key`, `should_resume`, `parse_command_line`.
- Produces:
  - `std::wstring win::widen(std::string_view utf8); std::string win::narrow(std::wstring_view);`
  - `std::filesystem::path win::app_data_dir();` → `%APPDATA%\VideoPlayer`
  - `class win::AppWindow { public: bool create(const vp::Settings&); HWND hwnd() const; void open_file(const std::string& path_utf8); vp::Player& player(); vp::Settings& settings(); void show_message(std::wstring text); };`
  - Window messages private to the app: `WM_APP_PLAYER_WAKEUP = WM_APP + 1` (posted by the player wakeup callback).

Behaviour to implement:
- **Startup order in `wWinMain`:** parse command line → `load_settings` → create and show the main window at `fit_rect_to_work_areas(settings.window, monitor work areas, {1280, 720})` → construct `Player` with the video child window's HWND as `wid` → `open_file` if a file was given → message loop. The resume store is loaded lazily inside `open_file` (it is needed for the start position) and nothing else is loaded before the first frame.
- The video area is a child window filling the client area; call `EnableWindow(child, FALSE)` so mouse input reaches the main window (verify this works with mpv's `wid` output; if mpv creates its own grandchild window, disable that too).
- `open_file`: saves the current file's position first (see below), looks up `resume_key(path)` in the store, calls `player.load(path, start)`, sets the title to `<file name> - VideoPlayer`.
- Position saving: on file switch and on window close, if `should_resume(position, duration)` then `set`, else `clear`; then `save()`. On `EndOfFile`: `clear` and `save`; the player stays paused on the last frame.
- `LoadFailed`: `show_message(L"Can't play this file")` — text centred in the window, no dialog.
- No file: black window with a centred "Open file" button (standard open dialog, all files filter) and `WS_EX_ACCEPTFILES` drag-and-drop; a drop also works while a video is playing. Ctrl+O opens the dialog.
- On close: store window rect (the restored rect, not the fullscreen/maximised one), volume, layout, borderless in settings and save.
- Dark title bar via `DWMWA_USE_IMMERSIVE_DARK_MODE` when Windows is in dark mode (full theming comes in Task 12).

- [ ] **Step 1:** Implement `paths` and `AppWindow`, rewrite `main.cpp` in the startup order above.
- [ ] **Step 2:** Build, then run `VideoPlayer.exe bench\media\sample.mkv`. Expected: video plays with sound in the window; closing and reopening resumes near the previous position; after it plays to the end, reopening starts from 0.
- [ ] **Step 3:** Run with `bench\media\broken.mkv` → in-window message. Run with no arguments → "Open file" button works; dropping `sample.mkv` plays it. Copy `sample.mkv` to `bench\media\Árvíztűrő tükörfúrógép.mkv` and open it → plays and resumes.
- [ ] **Step 4:** Move the window to a second monitor position by editing `settings.ini` to `window=9000,9000,800,600`, start → window appears centred on the primary monitor.
- [ ] **Step 5:** Commit: `feat(win): main window with playback, resume and settings`.

### Task 7: Startup benchmark

**Files:**
- Create: `win/bench.{h,cpp}`, `bench/run.ps1`
- Modify: `win/main.cpp`, `win/app_window.cpp`

**Interfaces:**
- Consumes: `CommandLine::bench`, `PlayerEvent::FirstFrame`.
- Produces: `void win::bench_report_first_frame();` — computes milliseconds from the process creation time (`GetProcessTimes`) to now, after one `DwmFlush()`, appends the integer as a line to `%TEMP%\videoplayer-bench.txt`, then posts `WM_CLOSE`. Called by `AppWindow` on the first `FirstFrame` when `--bench` was given. In bench mode nothing is written to settings or the resume store.
- `bench/run.ps1 [-File bench\media\sample.mkv] [-Runs 10] [-LimitMs 300]`: deletes the result file, does one untimed warm-up run, launches the release exe `Runs` times with `--bench` waiting for each to exit, prints each value and the median, exits 1 if the median exceeds `LimitMs`.

- [ ] **Step 1:** Implement, build release.
- [ ] **Step 2:** Run `pwsh bench/run.ps1`. Expected: 10 values and a median printed. Record the median in the commit message.
- [ ] **Step 3:** If the median is over 300 ms, profile before going on: log timestamps at window shown, mpv initialised, `loadfile` sent, `FileLoaded`, `FirstFrame`, and cut what dominates (candidates: move `mpv_create`/`mpv_initialize` to a thread started before window creation; delay-load of anything non-essential; `vo` and `gpu-context` choices; `demuxer-lavf-probesize`/`analyzeduration` only if probing dominates). Re-run until ≤ 300 ms or report the floor and what it consists of.
- [ ] **Step 4:** Commit: `feat: startup benchmark (median NNN ms)`.

From here on, every task ends by running `pwsh bench/run.ps1`; a regression over the limit must be fixed before committing.

### Task 8: Single instance

**Files:**
- Create: `win/single_instance.{h,cpp}`
- Modify: `win/main.cpp`, `win/app_window.cpp`

**Interfaces:**
- Produces:
  - `bool win::forward_to_running_instance(const std::optional<std::string>& file_utf8);` — if the named mutex `Local\VideoPlayer.SingleInstance` already exists, finds the main window by class name `VideoPlayerMainWindow`, sends `WM_COPYDATA` (`dwData = 1`, payload = absolute path as UTF-16, or empty), calls `AllowSetForegroundWindow` for it first, returns true. Otherwise acquires the mutex and returns false.
  - `AppWindow` handles `WM_COPYDATA` with `dwData == 1`: restores if minimised, brings itself to the foreground, and calls `open_file` if the payload is non-empty.
- The path is made absolute in the *second* process (its working directory may differ). `--bench` runs skip single-instance entirely.

- [ ] **Step 1:** Implement; `forward_to_running_instance` is the first thing `wWinMain` does after parsing the command line.
- [ ] **Step 2:** Manual check: play `sample.mkv`, then from a different working directory launch the exe with a relative path to `no-audio.mp4` → the same window switches file and comes to front; Task Manager shows one process. Repeat with the first window minimised, and (after Task 9) fullscreen. Repeat with the `Árvíztűrő…` file.
- [ ] **Step 3:** Run the benchmark; commit: `feat(win): single instance with file hand-over`.

### Task 9: Keyboard, mouse, fullscreen, borderless

**Files:**
- Modify: `win/app_window.{h,cpp}`

**Interfaces:**
- Produces on `AppWindow`: `void toggle_fullscreen(); bool is_fullscreen() const; void set_borderless(bool); bool is_borderless() const; void cycle_speed(int direction); void cycle_audio(); void cycle_subtitle();` (used by the control bar and menu later).

Values:
- Keys: Space play/pause; Left/Right seek ∓5 s; Ctrl+Left/Right ∓60 s; Up/Down volume ±5; M mute; F and F11 fullscreen; Esc leaves fullscreen; B borderless; L switch control layout; S cycle subtitle (including off); A cycle audio track; `[` / `]` speed down/up through 0.5, 0.75, 1, 1.25, 1.5, 2; Ctrl+O open.
- Mouse on the video: double-click toggles fullscreen; left-drag moves the window (send `WM_NCLBUTTONDOWN`/`HTCAPTION`; not in fullscreen); wheel changes volume ±5.
- Fullscreen: borderless popup covering the current monitor, previous placement and style restored on exit; not persisted.
- Borderless: remove caption and frame, keep the window resizable by returning resize hit-test codes from `WM_NCHITTEST` within 6 px of the edges and keep the DWM shadow; persisted in settings and applied at startup before the window is shown.
- The cursor hides after 2 s without mouse movement while playing in fullscreen or borderless.

- [ ] **Step 1:** Implement.
- [ ] **Step 2:** Manual check of every key and mouse action above against `sample.mkv`, including borderless → resize from each edge → restart (still borderless), and fullscreen on a second monitor if available.
- [ ] **Step 3:** Run the benchmark; commit: `feat(win): input, fullscreen and borderless`.

### Task 10: Bar layout logic and docked control bar

**Files:**
- Create: `core/include/vp/bar_layout.h`, `core/src/bar_layout.cpp`, `win/control_bar.{h,cpp}`
- Modify: `win/app_window.{h,cpp}`
- Test: `tests/test_bar_layout.cpp`

**Interfaces:**
- Produces (core):

```cpp
namespace vp {
enum class BarElement { None, PlayPause, Time, Seek, Volume, Subtitles, Audio, Speed, Borderless, Fullscreen };
struct RectF { float x, y, w, h; };
enum class BarStyle { Docked, Floating };   // Docked: one row. Floating: seek bar on its own row above the buttons.
struct BarLayout { RectF play, time, seek, volume, subtitles, audio, speed, borderless, fullscreen; };
BarLayout compute_bar_layout(BarStyle style, float width, float height);
BarElement hit_test(const BarLayout&, float x, float y);
double seek_fraction(const BarLayout&, float x);   // 0..1, clamped
}
```

- Sizes (device-independent pixels): buttons 32×32 with 4 gap; time text 110 wide; volume slider 80 wide; outer padding 12. Docked height 48; floating height 72. When `width` is too small, the volume slider is dropped first (zero-size rect), then the time text; the seek bar never gets narrower than 80.
- Produces (win): `class win::ControlBar { public: void attach(HWND host, vp::BarStyle style, AppWindow& app); void on_size(); void on_paint(); bool on_mouse(UINT msg, WPARAM, LPARAM); void update(const vp::PlayerState&); void set_theme(const Theme&); };` — draws with a Direct2D `ID2D1HwndRenderTarget` and DirectWrite; icons are glyphs from the "Segoe Fluent Icons" font; seek and volume support click and drag; hovering the seek bar shows the target time in a small tooltip-style label; clicking Subtitles/Audio/Speed opens a popup menu anchored to the button (the menu itself comes from Task 12; until then these three buttons call `cycle_*`).
- Direct2D factory, render target and text formats are created on first paint, never before the first video frame.
- `AppWindow` in Docked layout: a 48-DIP child strip below the video child; hidden in fullscreen and borderless (the floating panel replaces it there, Task 11).

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE("docked layout is one row, in order, inside bounds") {
    auto l = vp::compute_bar_layout(vp::BarStyle::Docked, 900, 48);
    CHECK(l.play.x < l.time.x); CHECK(l.time.x < l.seek.x); CHECK(l.seek.x < l.volume.x);
    CHECK(l.volume.x < l.subtitles.x); CHECK(l.subtitles.x < l.audio.x); CHECK(l.audio.x < l.speed.x);
    CHECK(l.speed.x < l.borderless.x); CHECK(l.borderless.x < l.fullscreen.x);
    CHECK(l.fullscreen.x + l.fullscreen.w <= 900 - 12 + 0.5f);
    CHECK(l.play.w == 32); CHECK(l.volume.w == 80);
}
TEST_CASE("floating layout puts seek on its own full-width row above the buttons") {
    auto l = vp::compute_bar_layout(vp::BarStyle::Floating, 720, 72);
    CHECK(l.seek.y + l.seek.h <= l.play.y); CHECK(l.seek.w == doctest::Approx(720 - 24));
}
TEST_CASE("narrow widths drop volume, then time, and keep seek >= 80") {
    auto a = vp::compute_bar_layout(vp::BarStyle::Docked, 420, 48); CHECK(a.volume.w == 0); CHECK(a.time.w > 0);
    auto b = vp::compute_bar_layout(vp::BarStyle::Docked, 330, 48); CHECK(b.time.w == 0); CHECK(b.seek.w >= 80);
}
TEST_CASE("hit testing and seek fraction") {
    auto l = vp::compute_bar_layout(vp::BarStyle::Docked, 900, 48);
    CHECK(vp::hit_test(l, l.play.x + 5, l.play.y + 5) == vp::BarElement::PlayPause);
    CHECK(vp::hit_test(l, l.seek.x + l.seek.w / 2, l.seek.y + l.seek.h / 2) == vp::BarElement::Seek);
    CHECK(vp::hit_test(l, 1, 1) == vp::BarElement::None);
    CHECK(vp::seek_fraction(l, l.seek.x - 50) == 0.0);
    CHECK(vp::seek_fraction(l, l.seek.x + l.seek.w / 2) == doctest::Approx(0.5));
    CHECK(vp::seek_fraction(l, 5000) == 1.0);
}
```

- [ ] **Step 2:** Run `ctest`. Expected: compile failure.
- [ ] **Step 3:** Implement `bar_layout`; run `ctest`; expected PASS.
- [ ] **Step 4:** Implement `ControlBar` and the docked strip; temporarily force Docked layout.
- [ ] **Step 5:** Manual check: every button works, seek and volume drag smoothly, time text updates, the bar reflows when the window is made narrow, 150% display scaling looks sharp.
- [ ] **Step 6:** Run the benchmark; commit: `feat: control bar with docked layout`.

### Task 11: Floating panel and layout switching

**Files:**
- Create: `win/floating_panel.{h,cpp}`
- Modify: `win/app_window.{h,cpp}`

**Interfaces:**
- Consumes: `win::ControlBar`, `vp::BarStyle::Floating`, `Settings::layout`.
- Produces: `class win::FloatingPanel { public: void create(AppWindow& owner); void reposition(); void show(); void hide_after_idle(); void set_theme(const Theme&); ControlBar& bar(); };` and `void AppWindow::set_layout(vp::ControlLayout);`

Values:
- The panel is a `WS_POPUP` window owned by the main window (`WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`), so it stays above the video, moves with it, and never takes focus or appears in the taskbar. Backdrop: `DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_TRANSIENTWINDOW` (acrylic) with the client area extended into the frame; corners `DWMWCP_ROUND`. Verify acrylic renders on a `WS_POPUP`; if it does not, fall back to a solid semi-opaque fill drawn in Direct2D with the same rounded shape.
- Geometry: width `min(720, 80% of the video width)`, height 72, centred horizontally, 16 above the bottom edge of the video. `reposition()` runs on main-window move, size, and show/hide; the panel hides when the main window is minimised.
- Visibility: shown on mouse movement over the video or the panel; hidden 2 s after the last movement while playing, unless the mouse is over the panel or a menu is open; always shown while paused or with no file. Fade in/out 150 ms using layered-window alpha (or `AnimateWindow` if alpha conflicts with the backdrop).
- The panel window is created lazily on first need, after the first frame.
- `set_layout`: Floating → docked strip hidden, panel active. Docked → strip shown and panel unused in a normal window; in fullscreen or borderless the strip is hidden and the panel is used. The choice is stored in settings. Key `L` and the menu call `set_layout`.

- [ ] **Step 1:** Implement.
- [ ] **Step 2:** Manual check: default is Floating; panel follows the window when dragged and resized, fades after 2 s of playing, stays while paused; `L` switches to Docked and back; Docked + fullscreen and Docked + borderless show the panel on mouse move; alt-tab away and back leaves no orphan panel; restart keeps the chosen layout.
- [ ] **Step 3:** Run the benchmark; commit: `feat(win): floating control panel and layout switching`.

### Task 12: Context menu and theme

**Files:**
- Create: `win/context_menu.{h,cpp}`, `win/theme.{h,cpp}`
- Modify: `win/app_window.cpp`, `win/control_bar.cpp`, `win/floating_panel.cpp`

**Interfaces:**
- Produces:
  - `struct win::Theme { bool dark; D2D1_COLOR_F accent, text, text_dim, bar_fill, track_fill; };`
  - `win::Theme win::current_theme();` — dark/light from `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`, accent from `DwmGetColorizationColor`.
  - `enum class win::MenuKind { Full, Subtitles, Audio, Speed };`
  - `void win::show_menu(AppWindow& app, MenuKind kind, POINT screen_pt);`
- Full menu contents, in order: Open file… (Ctrl+O) | Play/Pause | Subtitles ▸ (Off, then each subtitle track) | Audio track ▸ (each audio track) | Speed ▸ (0.5×, 0.75×, 1×, 1.25×, 1.5×, 2×) | Controls ▸ (Floating, Docked) | Borderless (B) | Fullscreen (F). Current selections carry a radio check. A track's label is its title if present, else its language, else "Track N"; external subtitle files get " (file)" appended.
- Right-click on the video opens `Full` at the cursor; the bar's Subtitles/Audio/Speed buttons open the matching single submenu above the button (replacing the temporary `cycle_*` wiring from Task 10).
- Theme: applied to the control bar colours, the seek/volume fill (accent), the title bar (`DWMWA_USE_IMMERSIVE_DARK_MODE`), and menus (dark menus via `SetPreferredAppMode`/`FlushMenuThemes` from uxtheme ordinals 135/136, resolved at runtime and skipped if unavailable). `WM_SETTINGCHANGE` ("ImmersiveColorSet") and `WM_DWMCOLORIZATIONCOLORCHANGED` re-read the theme and repaint.

- [ ] **Step 1:** Implement.
- [ ] **Step 2:** Manual check: every menu item acts and shows the right check mark; switching audio and subtitle tracks on `sample.mkv` works, including the external `sample.srt`; toggling Windows between dark and light and changing the accent colour updates the running player without restart.
- [ ] **Step 3:** Run the benchmark; commit: `feat(win): context menu and system theme`.

### Task 13: Checklist, README, final verification

**Files:**
- Create: `docs/manual-checklist.md`, `README.md`

- [ ] **Step 1:** Write `docs/manual-checklist.md` covering: both layouts; borderless (drag, resize, persisted); fullscreen (double-click, F, Esc); audio and subtitle switching; subtitle file next to the video; speed; volume (slider, wheel, keys, persisted); resume and end-of-file clearing; second file replaces the first (normal, minimised, fullscreen); no-file window and drag-and-drop; unplayable file message; audio-only and no-audio files; non-ASCII file name; off-screen saved position; dark/light and accent change; 150% scaling.
- [ ] **Step 2:** Write `README.md`: what it is, how to build (`pwsh scripts/fetch-mpv.ps1`, the three CMake commands), how to run the benchmark, where settings live, how to set "Open with", the keyboard shortcuts, and the libmpv GPL note from the spec.
- [ ] **Step 3:** Clean build from scratch (`build/` deleted), run `ctest`, run `pwsh bench/run.ps1`, walk the whole checklist with real-world files in at least H.264, HEVC, AV1 and VP9 (confirm hardware decoding via mpv's `hwdec-current` property, logged in a debug build).
- [ ] **Step 4:** Confirm the release folder contains only `VideoPlayer.exe` and `libmpv-2.dll` and runs from a different directory on its own.
- [ ] **Step 5:** Commit: `docs: README and manual checklist`.
