#pragma once
#include <windows.h>

#include "control_bar.h"

namespace win {

class AppWindow;

// The floating control layout: a small rounded, translucent window owned by the main window
// and kept over the bottom of the video. It never takes focus.
class FloatingPanel {
public:
    explicit FloatingPanel(AppWindow& app);
    ~FloatingPanel();

    bool create(HINSTANCE instance);
    void reposition();              // follow the video area
    void set_visible(bool visible); // fades in or out
    bool mouse_over() const;
    void theme_changed();
    void dpi_changed();
    ControlBar& bar() { return bar_; }

private:
    static LRESULT CALLBACK Proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void apply_backdrop();

    AppWindow& app_;
    ControlBar bar_;
    HWND hwnd_ = nullptr;
    bool want_visible_ = false;
    int alpha_ = 0;
};

}  // namespace win
