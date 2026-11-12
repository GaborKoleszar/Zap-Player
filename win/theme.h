#pragma once
#include <windows.h>
#include <d2d1.h>

namespace win {

struct Theme {
    bool dark = true;
    D2D1_COLOR_F accent{0.38f, 0.80f, 1.0f, 1.0f};
    D2D1_COLOR_F text{1, 1, 1, 1};
    D2D1_COLOR_F text_dim{1, 1, 1, 0.65f};
    D2D1_COLOR_F panel_fill{0.17f, 0.17f, 0.17f, 0.55f}; // tint over the floating panel's backdrop
    D2D1_COLOR_F track_fill{1, 1, 1, 0.25f};
    D2D1_COLOR_F hover_fill{1, 1, 1, 0.10f};
};

// Dark/light mode and accent colour as currently set in Windows.
Theme current_theme();

// Dark or light title bar to match the theme.
void apply_title_bar_theme(HWND hwnd, const Theme& theme);

// Gives the window the Windows 11 Mica backdrop. It shows in the title bar and wherever the
// client area is painted fully transparent (the docked bar and the empty video area).
void apply_mica(HWND hwnd);

// Makes popup menus follow dark mode. Safe to call repeatedly.
void apply_menu_theme(const Theme& theme);

}  // namespace win
