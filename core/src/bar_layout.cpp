#include "vp/bar_layout.h"

#include <algorithm>

namespace vp {
namespace {

constexpr float kPad = 12;       // outer padding
constexpr float kGap = 4;        // between buttons
constexpr float kGroupGap = 8;   // between groups
constexpr float kButton = 32;
constexpr float kSpeedWidth = 44;  // holds text such as "1.25x"
constexpr float kTimeWidth = 110;
constexpr float kVolumeWidth = 80;
constexpr float kMinSeek = 80;

}  // namespace

BarLayout compute_bar_layout(BarStyle style, float width, float height) {
    BarLayout l;
    const float row_y = style == BarStyle::Docked ? (height - kButton) / 2 : height - 8 - kButton;

    // Right-hand group, laid out from the right edge inwards.
    float x = width - kPad;
    auto place_right = [&](float w) {
        x -= w;
        RectF r{x, row_y, w, kButton};
        x -= kGap;
        return r;
    };
    l.fullscreen = place_right(kButton);
    l.borderless = place_right(kButton);
    l.speed = place_right(kSpeedWidth);
    l.audio = place_right(kButton);
    l.subtitles = place_right(kButton);
    const float right_start = l.subtitles.x;

    l.play = {kPad, row_y, kButton, kButton};
    float left = kPad + kButton + kGroupGap;
    const float avail = right_start - kGroupGap - left;

    if (style == BarStyle::Docked) {
        const bool show_time = avail >= kTimeWidth + kGroupGap + kMinSeek;
        const bool show_volume = avail >= kTimeWidth + kGroupGap + kMinSeek + kGroupGap + kVolumeWidth;
        if (show_time) {
            l.time = {left, row_y, kTimeWidth, kButton};
            left += kTimeWidth + kGroupGap;
        }
        float seek_end = right_start - kGroupGap;
        if (show_volume) {
            l.volume = {seek_end - kVolumeWidth, row_y, kVolumeWidth, kButton};
            seek_end -= kVolumeWidth + kGroupGap;
        }
        l.seek = {left, row_y, std::max(seek_end - left, 0.0f), kButton};
    } else {
        l.seek = {kPad, 8, std::max(width - 2 * kPad, 0.0f), 20};
        if (avail >= kTimeWidth) l.time = {left, row_y, kTimeWidth, kButton};
        if (avail >= kTimeWidth + kGroupGap + kVolumeWidth)
            l.volume = {right_start - kGroupGap - kVolumeWidth, row_y, kVolumeWidth, kButton};
    }
    return l;
}

BarElement hit_test(const BarLayout& l, float x, float y) {
    if (l.play.contains(x, y)) return BarElement::PlayPause;
    if (l.seek.contains(x, y)) return BarElement::Seek;
    if (l.volume.contains(x, y)) return BarElement::Volume;
    if (l.subtitles.contains(x, y)) return BarElement::Subtitles;
    if (l.audio.contains(x, y)) return BarElement::Audio;
    if (l.speed.contains(x, y)) return BarElement::Speed;
    if (l.borderless.contains(x, y)) return BarElement::Borderless;
    if (l.fullscreen.contains(x, y)) return BarElement::Fullscreen;
    if (l.time.contains(x, y)) return BarElement::Time;
    return BarElement::None;
}

double seek_fraction(const BarLayout& l, float x) {
    const float x0 = l.seek.x + kSeekTrackInset, x1 = l.seek.x + l.seek.w - kSeekTrackInset;
    if (x1 <= x0) return 0.0;
    return std::clamp(static_cast<double>(x - x0) / (x1 - x0), 0.0, 1.0);
}

}  // namespace vp
