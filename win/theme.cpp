#include "theme.h"

#include <dwmapi.h>

namespace win {
namespace {

bool read_dword(const wchar_t* key, const wchar_t* value, DWORD& out) {
    DWORD size = sizeof(out);
    return RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD, nullptr, &out, &size) == ERROR_SUCCESS;
}

D2D1_COLOR_F mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0f};
}

}  // namespace

Theme current_theme() {
    Theme t;
    DWORD light = 0;
    if (read_dword(L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", light))
        t.dark = light == 0;

    D2D1_COLOR_F accent{0.0f, 0.47f, 0.83f, 1.0f};
    DWORD abgr = 0;
    if (read_dword(L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", abgr)) {
        accent = {(abgr & 0xFF) / 255.0f, ((abgr >> 8) & 0xFF) / 255.0f, ((abgr >> 16) & 0xFF) / 255.0f, 1.0f};
    } else {
        DWORD argb = 0;
        BOOL opaque = FALSE;
        if (SUCCEEDED(DwmGetColorizationColor(&argb, &opaque)))
            accent = {((argb >> 16) & 0xFF) / 255.0f, ((argb >> 8) & 0xFF) / 255.0f, (argb & 0xFF) / 255.0f, 1.0f};
    }

    if (t.dark) {
        // Windows uses a lighter shade of the accent for controls on dark surfaces.
        t.accent = mix(accent, {1, 1, 1, 1}, 0.45f);
        t.text = {1, 1, 1, 1};
        t.text_dim = {1, 1, 1, 0.65f};
        t.panel_fill = {0.17f, 0.17f, 0.17f, 0.55f};
        t.track_fill = {1, 1, 1, 0.25f};
        t.hover_fill = {1, 1, 1, 0.10f};
    } else {
        t.accent = accent;
        t.text = {0.1f, 0.1f, 0.1f, 1};
        t.text_dim = {0.1f, 0.1f, 0.1f, 0.65f};
        t.panel_fill = {0.96f, 0.96f, 0.96f, 0.60f};
        t.track_fill = {0, 0, 0, 0.22f};
        t.hover_fill = {0, 0, 0, 0.07f};
    }
    return t;
}

void apply_title_bar_theme(HWND hwnd, const Theme& theme) {
    BOOL dark = theme.dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
}

void apply_mica(HWND hwnd) {
    int backdrop = 2;  // DWMSBT_MAINWINDOW (Mica). Ignored on Windows builds that lack it.
    DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &backdrop, sizeof(backdrop));
    MARGINS margins{-1, -1, -1, -1};  // the whole client area is "glass"
    DwmExtendFrameIntoClientArea(hwnd, &margins);
}

void apply_menu_theme(const Theme& theme) {
    // uxtheme exports these by ordinal only. If they are missing, menus simply stay light.
    static HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!uxtheme) return;
    using SetPreferredAppModeFn = int(WINAPI*)(int);
    using FlushMenuThemesFn = void(WINAPI*)();
    auto set_mode = reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
    auto flush = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
    if (!set_mode || !flush) return;
    set_mode(theme.dark ? 2 /* ForceDark */ : 3 /* ForceLight */);
    flush();
}

}  // namespace win
