#pragma once

namespace vp {

enum class BarElement { None, PlayPause, Time, Seek, Volume, Subtitles, Audio, Speed, Borderless, Fullscreen };

struct RectF {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return w > 0 && h > 0 && px >= x && px < x + w && py >= y && py < y + h; }
};

// Docked: everything on one row. Floating: the seek bar on its own row above the buttons.
enum class BarStyle { Docked, Floating };

inline constexpr float kDockedBarHeight = 48;
inline constexpr float kFloatingBarHeight = 72;

struct BarLayout {
    RectF play, time, seek, volume, subtitles, audio, speed, borderless, fullscreen;
};

// All values are device-independent pixels. When the bar is too narrow the volume slider is
// dropped first (zero-size rect), then the time text.
BarLayout compute_bar_layout(BarStyle style, float width, float height);

BarElement hit_test(const BarLayout& layout, float x, float y);

// The seek track is drawn this much shorter than the seek area on each side, so that the thumb
// fits at both ends.
inline constexpr float kSeekTrackInset = 6;

// Position along the drawn seek track as 0..1, clamped. Hover preview, click and drag all use
// this, so the time shown under the pointer is the time a click there seeks to.
double seek_fraction(const BarLayout& layout, float x);

}  // namespace vp
