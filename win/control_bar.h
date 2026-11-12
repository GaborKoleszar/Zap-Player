#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include <vp/bar_layout.h>

namespace win {

class AppWindow;

// Draws the playback controls into a host window with Direct2D and handles the mouse on them.
// Used by both layouts: the docked strip and the floating panel.
class ControlBar {
public:
    // `tinted` adds a light fill over the backdrop; otherwise the bar is clear.
    ControlBar(AppWindow& app, vp::BarStyle style, bool tinted);
    ~ControlBar();

    void attach(HWND host) { host_ = host; }
    void set_tinted(bool tinted) { tinted_ = tinted; }
    void paint();           // call from the host's WM_PAINT
    void resize();          // call from the host's WM_SIZE
    void discard();         // drop device resources (theme or DPI change)
    void invalidate();
    // Returns true when the message was used. Handles move, leave, button down/up.
    bool handle_mouse(UINT msg, WPARAM wp, LPARAM lp);
    bool dragging() const { return drag_ != vp::BarElement::None; }

private:
    bool ensure_resources();
    vp::BarLayout layout() const;
    D2D1_RECT_F rect(const vp::RectF& r) const { return {r.x, r.y, r.x + r.w, r.y + r.h}; }
    void draw_glyph(const vp::RectF& r, wchar_t glyph, vp::BarElement element);
    void draw_button_text(const vp::RectF& r, const wchar_t* text, vp::BarElement element);
    void draw_slider(float x0, float x1, float cy, double fraction, bool active);
    void activate(vp::BarElement element);
    void apply_drag(float x);
    float volume_slider_start(const vp::BarLayout& l) const { return l.volume.x + 26; }

    AppWindow& app_;
    vp::BarStyle style_;
    bool tinted_;
    HWND host_ = nullptr;

    ID2D1HwndRenderTarget* target_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    IDWriteTextFormat* text_format_ = nullptr;
    IDWriteTextFormat* icon_format_ = nullptr;

    vp::BarElement hover_ = vp::BarElement::None;
    vp::BarElement pressed_ = vp::BarElement::None;
    vp::BarElement drag_ = vp::BarElement::None;
    double drag_fraction_ = 0;
    float hover_x_ = 0;
    bool tracking_leave_ = false;
    ULONGLONG last_drag_seek_ = 0;
};

}  // namespace win
