# Manual checklist

Run `pwsh bench/make-sample.ps1` first; the files below are in `bench/media/`.

## Start-up
- [ ] `pwsh bench/run.ps1` reports a median under the limit.
- [ ] Double-clicking a video in Explorer (after setting "Open with") plays it.

## Controls
- [ ] Floating layout: panel appears on mouse move, fades about 2 s after the mouse stops while playing, stays while paused.
- [ ] Docked layout (L): bar sits under the video and never covers it; choice survives a restart.
- [ ] Docked + fullscreen and docked + borderless: the bar is replaced by the floating panel.
- [ ] Enter and leave fullscreen several times in a row (docked and floating layout): the panel shows on mouse move every time.
- [ ] Dragging the seek bar on a large file follows the pointer and lands exactly where released.
- [ ] Every bar button works: play/pause, seek (click and drag), volume (drag, speaker icon mutes), subtitles, audio, speed, borderless, fullscreen.
- [ ] Hovering the seek bar shows the time under the pointer in the time label.
- [ ] Narrow window: volume slider disappears first, then the time, the rest stays usable.
- [ ] Right-click menu: every item works and shows the right check mark.
- [ ] Menu > Settings > "Always start with defaults": change volume, layout and window size, restart: all back to defaults, and the menu choice itself is kept. Back on "Remember last settings", changes survive a restart again.

## Window
- [ ] Borderless (B): no title bar; drag the video to move, drag edges to resize; survives a restart.
- [ ] Fullscreen: F, F11, double-click; Esc leaves; pointer hides with the controls.
- [ ] Window size and position are restored on the next start; a maximised window reopens maximised.
- [ ] Fullscreen moved to a monitor with different scaling (Win+Shift+Arrow) still covers it.
- [ ] Edit `%APPDATA%\Zap\settings.ini` to `window=9000,9000,800,600`: the window opens on the primary monitor.
- [ ] 125 % / 150 % display scaling: text and icons are sharp.

## Playback
- [ ] `sample.mkv`: two audio tracks (A or the menu switches between tones), embedded subtitle and `sample.srt` both listed; the `.srt` shows by default.
- [ ] Subtitles move up while the floating panel is visible and back down when it hides.
- [ ] Speed: `[` `]`, numpad - / +, and the menu.
- [ ] Volume: slider, wheel, Up/Down, M; level survives a restart.
- [ ] `long.mp4`: close at about 0:30, reopen: resumes there. Let it play to the end, reopen: starts at 0:00.
- [ ] At the end the last frame stays; Space plays again from the start.
- [ ] `audio-only.m4a` plays with a black window and working controls; `no-audio.mp4` plays silently.
- [ ] `broken.mkv`: "Can't play this file" above the "Open file" button, no dialog; the button and opening a good file afterwards work.
- [ ] `Árvíztűrő tükörfúrógép.mkv` opens, shows its name in the title, and resumes.
- [ ] Real files in H.264, HEVC, AV1 and VP9 play; with `VP_MPV_LOG` set, the log says "Using hardware decoding".

## Files
- [ ] No file: dark window with "Open file"; the button and Ctrl+O open the dialog; dropping a file plays it.
- [ ] Dropping a file while another plays replaces it.
- [ ] Opening a second video from Explorer while one plays: same window, comes to the front (also when the window is minimised or fullscreen); Task Manager shows one process.

## Theme
- [ ] Mica: title bar, docked bar and the empty window are one wallpaper-tinted surface while the window is active; video and its black bars stay opaque.
- [ ] Floating panel (also in fullscreen): the video shows through it, blurred and tinted, while playing.
- [ ] Switch Windows between dark and light: title bar, docked bar, floating panel and menus follow without a restart.
- [ ] Change the accent colour: seek and volume fill follow.
