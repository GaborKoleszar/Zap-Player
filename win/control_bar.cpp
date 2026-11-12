#include "control_bar.h"

#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include <vp/time_format.h>

#include "app_window.h"
#include "context_menu.h"
#include "d2d.h"
#include "paths.h"

namespace win {
namespace {

// Segoe Fluent Icons code points.
constexpr wchar_t kGlyphPlay = 0xE768;
constexpr wchar_t kGlyphPause = 0xE769;
constexpr wchar_t kGlyphVolume = 0xE767;
constexpr wchar_t kGlyphMute = 0xE74F;
constexpr wchar_t kGlyphSubtitles = 0xE7F0;
constexpr wchar_t kGlyphAudio = 0xE8D6;
constexpr wchar_t kGlyphBorderless = 0xE922;
constexpr wchar_t kGlyphBordered = 0xE923;
constexpr wchar_t kGlyphFullscreen = 0xE740;
constexpr wchar_t kGlyphExitFullscreen = 0xE73F;

std::wstring speed_label(double speed) {
    wchar_t buf[16];
    std::swprintf(buf, 16, L"%g×", speed);
    return buf;
}

}  // namespace

ControlBar::ControlBar(AppWindow& app, vp::BarStyle style, bool tinted)
    : app_(app), style_(style), tinted_(tinted) {}

ControlBar::~ControlBar() {
    discard();
    release(text_format_);
    release(icon_format_);
}

void ControlBar::discard() {
    release(brush_);
    release(target_);
}

void ControlBar::invalidate() {
    if (host_ && IsWindowVisible(host_)) InvalidateRect(host_, nullptr, FALSE);
}

void ControlBar::resize() { resize_window_target(target_, host_); }

bool ControlBar::ensure_resources() {
    if (!dwrite_factory() || !ensure_window_target(host_, app_.dpi(), target_, brush_)) return false;
    if (!text_format_) {
        dwrite_factory()->CreateTextFormat(L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.5f, L"",
                                           &text_format_);
        if (text_format_) {
            text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }
    if (!icon_format_) {
        dwrite_factory()->CreateTextFormat(L"Segoe Fluent Icons", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"",
                                           &icon_format_);
        if (icon_format_) {
            icon_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            icon_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }
    return target_ && brush_ && text_format_ && icon_format_;
}

vp::BarLayout ControlBar::layout() const {
    RECT rc;
    GetClientRect(host_, &rc);
    float scale = 96.0f / static_cast<float>(app_.dpi());
    return vp::compute_bar_layout(style_, rc.right * scale, rc.bottom * scale);
}

void ControlBar::draw_glyph(const vp::RectF& r, wchar_t glyph, vp::BarElement element) {
    if (r.w <= 0) return;
    const Theme& t = app_.theme();
    if (element != vp::BarElement::None && (hover_ == element || pressed_ == element)) {
        brush_->SetColor(t.hover_fill);
        target_->FillRoundedRectangle(D2D1::RoundedRect(rect(r), 4, 4), brush_);
    }
    brush_->SetColor(t.text);
    target_->DrawText(&glyph, 1, icon_format_, rect(r), brush_);
}

void ControlBar::draw_button_text(const vp::RectF& r, const wchar_t* text, vp::BarElement element) {
    if (r.w <= 0) return;
    const Theme& t = app_.theme();
    if (hover_ == element || pressed_ == element) {
        brush_->SetColor(t.hover_fill);
        target_->FillRoundedRectangle(D2D1::RoundedRect(rect(r), 4, 4), brush_);
    }
    brush_->SetColor(t.text);
    text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    target_->DrawText(text, static_cast<UINT32>(wcslen(text)), text_format_, rect(r), brush_);
}

void ControlBar::draw_slider(float x0, float x1, float cy, double fraction, bool active) {
    if (x1 <= x0) return;
    const Theme& t = app_.theme();
    fraction = std::clamp(fraction, 0.0, 1.0);
    float xf = x0 + static_cast<float>((x1 - x0) * fraction);
    brush_->SetColor(t.track_fill);
    target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x0, cy - 2, x1, cy + 2), 2, 2), brush_);
    brush_->SetColor(t.accent);
    target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x0, cy - 2, xf, cy + 2), 2, 2), brush_);
    float radius = active ? 7.0f : 5.0f;
    target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(xf, cy), radius, radius), brush_);
}

void ControlBar::paint() {
    PAINTSTRUCT ps;
    BeginPaint(host_, &ps);
    EndPaint(host_, &ps);
    if (!ensure_resources()) return;

    const Theme& t = app_.theme();
    const vp::PlayerState& s = app_.player().state();
    const vp::BarLayout l = layout();

    target_->BeginDraw();
    // Both layouts sit on a backdrop that shows through transparent pixels: Mica for the docked
    // bar, the blurred video for the floating panel (tinted here only if the blur is missing).
    if (tinted_) target_->Clear(t.panel_fill);
    else target_->Clear(D2D1::ColorF(0, 0, 0, 0));

    draw_glyph(l.play, (s.paused || !s.has_file || s.at_end) ? kGlyphPlay : kGlyphPause, vp::BarElement::PlayPause);

    // Seek bar. While dragging, show where the drag is rather than where playback is.
    double fraction = s.duration > 0 ? s.position / s.duration : 0;
    if (drag_ == vp::BarElement::Seek) fraction = drag_fraction_;
    bool seek_active = hover_ == vp::BarElement::Seek || drag_ == vp::BarElement::Seek;
    draw_slider(l.seek.x + vp::kSeekTrackInset, l.seek.x + l.seek.w - vp::kSeekTrackInset, l.seek.y + l.seek.h / 2,
                fraction, seek_active);

    if (l.time.w > 0) {
        // Hovering or dragging the seek bar previews the time under the pointer.
        double shown = s.position;
        if (drag_ == vp::BarElement::Seek) shown = drag_fraction_ * s.duration;
        else if (hover_ == vp::BarElement::Seek) shown = vp::seek_fraction(l, hover_x_) * s.duration;
        std::wstring text = widen(vp::format_time(shown) + " / " + vp::format_time(s.duration));
        brush_->SetColor(seek_active ? t.text : t.text_dim);
        text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        target_->DrawText(text.c_str(), static_cast<UINT32>(text.size()), text_format_, rect(l.time), brush_);
    }

    if (l.volume.w > 0) {
        vp::RectF icon{l.volume.x, l.volume.y, 24, l.volume.h};
        draw_glyph(icon, (s.muted || s.volume == 0) ? kGlyphMute : kGlyphVolume, vp::BarElement::None);
        double v = drag_ == vp::BarElement::Volume ? drag_fraction_ : s.volume / 100.0;
        draw_slider(volume_slider_start(l) + 4, l.volume.x + l.volume.w - 6, l.volume.y + l.volume.h / 2, v,
                    hover_ == vp::BarElement::Volume || drag_ == vp::BarElement::Volume);
    }

    draw_glyph(l.subtitles, kGlyphSubtitles, vp::BarElement::Subtitles);
    draw_glyph(l.audio, kGlyphAudio, vp::BarElement::Audio);
    draw_button_text(l.speed, speed_label(s.speed).c_str(), vp::BarElement::Speed);
    draw_glyph(l.borderless, app_.is_borderless() ? kGlyphBordered : kGlyphBorderless, vp::BarElement::Borderless);
    draw_glyph(l.fullscreen, app_.is_fullscreen() ? kGlyphExitFullscreen : kGlyphFullscreen,
               vp::BarElement::Fullscreen);

    if (target_->EndDraw() == D2DERR_RECREATE_TARGET) discard();
}

void ControlBar::apply_drag(float x) {
    const vp::BarLayout l = layout();
    if (drag_ == vp::BarElement::Seek) {
        drag_fraction_ = vp::seek_fraction(l, x);
        double duration = app_.player().state().duration;
        ULONGLONG now = GetTickCount64();
        if (duration > 0 && now - last_drag_seek_ >= 40) {
            // Keyframe seeks keep up with the pointer; the exact seek happens on release.
            last_drag_seek_ = now;
            app_.player().seek_absolute(drag_fraction_ * duration, vp::SeekMode::Fast);
        }
    } else if (drag_ == vp::BarElement::Volume) {
        float x0 = volume_slider_start(l) + 4, x1 = l.volume.x + l.volume.w - 6;
        drag_fraction_ = x1 > x0 ? std::clamp(static_cast<double>(x - x0) / (x1 - x0), 0.0, 1.0) : 0.0;
        app_.player().set_volume(static_cast<int>(drag_fraction_ * 100 + 0.5));
    }
}

void ControlBar::activate(vp::BarElement element) {
    const vp::BarLayout l = layout();
    float scale = static_cast<float>(app_.dpi()) / 96.0f;
    auto menu_point = [&](const vp::RectF& r) {
        POINT pt{static_cast<LONG>(r.x * scale), static_cast<LONG>(r.y * scale)};
        ClientToScreen(host_, &pt);
        return pt;
    };
    switch (element) {
        case vp::BarElement::PlayPause: app_.player().toggle_pause(); break;
        case vp::BarElement::Subtitles: show_menu(app_, MenuKind::Subtitles, menu_point(l.subtitles), true); break;
        case vp::BarElement::Audio: show_menu(app_, MenuKind::Audio, menu_point(l.audio), true); break;
        case vp::BarElement::Speed: show_menu(app_, MenuKind::Speed, menu_point(l.speed), true); break;
        case vp::BarElement::Borderless: app_.set_borderless(!app_.is_borderless()); break;
        case vp::BarElement::Fullscreen: app_.toggle_fullscreen(); break;
        default: break;
    }
}

bool ControlBar::handle_mouse(UINT msg, WPARAM, LPARAM lp) {
    float scale = 96.0f / static_cast<float>(app_.dpi());
    float x = GET_X_LPARAM(lp) * scale, y = GET_Y_LPARAM(lp) * scale;

    switch (msg) {
        case WM_MOUSEMOVE: {
            if (!tracking_leave_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, host_, 0};
                TrackMouseEvent(&tme);
                tracking_leave_ = true;
            }
            if (dragging()) {
                apply_drag(x);
                invalidate();
                return true;
            }
            vp::BarElement over = vp::hit_test(layout(), x, y);
            if (over != hover_ || over == vp::BarElement::Seek) {
                hover_ = over;
                hover_x_ = x;
                invalidate();
            }
            return true;
        }
        case WM_MOUSELEAVE:
            tracking_leave_ = false;
            if (hover_ != vp::BarElement::None || pressed_ != vp::BarElement::None) {
                hover_ = vp::BarElement::None;
                pressed_ = vp::BarElement::None;
                invalidate();
            }
            return true;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK: {
            const vp::BarLayout l = layout();
            vp::BarElement over = vp::hit_test(l, x, y);
            if (over == vp::BarElement::Seek) {
                if (!app_.player().state().has_file) return true;
                drag_ = over;
                SetCapture(host_);
                apply_drag(x);
            } else if (over == vp::BarElement::Volume) {
                if (x < volume_slider_start(l)) {
                    app_.player().toggle_mute();
                } else {
                    drag_ = over;
                    SetCapture(host_);
                    apply_drag(x);
                }
            } else {
                pressed_ = over;
            }
            invalidate();
            return true;
        }
        case WM_LBUTTONUP: {
            if (dragging()) {
                if (drag_ == vp::BarElement::Seek) {
                    double duration = app_.player().state().duration;
                    if (duration > 0) app_.player().seek_absolute(drag_fraction_ * duration, vp::SeekMode::Exact);
                }
                drag_ = vp::BarElement::None;
                ReleaseCapture();
                invalidate();
                return true;
            }
            vp::BarElement over = vp::hit_test(layout(), x, y);
            vp::BarElement was = pressed_;
            pressed_ = vp::BarElement::None;
            invalidate();
            if (was != vp::BarElement::None && was == over) activate(over);
            return true;
        }
        case WM_CAPTURECHANGED:
            drag_ = vp::BarElement::None;
            return true;
        default:
            return false;
    }
}

}  // namespace win
