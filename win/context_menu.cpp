#include "context_menu.h"

#include <cmath>
#include <cstdio>
#include <string>

#include "app_window.h"
#include "paths.h"

namespace win {
namespace {

enum : UINT {
    kIdOpen = 1,
    kIdPlayPause,
    kIdBorderless,
    kIdFullscreen,
    kIdLayoutFloating,
    kIdLayoutDocked,
    kIdStartupRemember,
    kIdStartupDefaults,
    kIdSubtitleOff = 100,
    kIdSubtitleBase = 101,   // + position in the subtitle list
    kIdAudioBase = 1000,     // + position in the audio list
    kIdSpeedBase = 2000,     // + index into kSpeeds
};

std::wstring track_label(const vp::Track& t, int number) {
    std::wstring label;
    if (!t.title.empty()) label = widen(t.title);
    if (!t.lang.empty()) label += label.empty() ? widen(t.lang) : L" (" + widen(t.lang) + L")";
    if (label.empty()) label = L"Track " + std::to_wstring(number);
    if (t.external && t.type == vp::TrackType::Subtitle) label += L" (file)";
    // A literal ampersand would otherwise become a menu accelerator.
    for (size_t i = 0; i < label.size(); ++i)
        if (label[i] == L'&') label.insert(i++, 1, L'&');
    return label;
}

void add_radio(HMENU menu, UINT id, const std::wstring& text, bool checked) {
    AppendMenuW(menu, MF_STRING, id, text.c_str());
    if (checked) CheckMenuRadioItem(menu, id, id, id, MF_BYCOMMAND);
}

void fill_subtitles(HMENU menu, const std::vector<vp::Track>& tracks) {
    bool any_selected = false;
    for (const auto& t : tracks)
        if (t.type == vp::TrackType::Subtitle && t.selected) any_selected = true;
    add_radio(menu, kIdSubtitleOff, L"Off", !any_selected);
    int n = 0;
    for (const auto& t : tracks) {
        if (t.type != vp::TrackType::Subtitle) continue;
        add_radio(menu, kIdSubtitleBase + n, track_label(t, n + 1), t.selected);
        ++n;
    }
}

void fill_audio(HMENU menu, const std::vector<vp::Track>& tracks) {
    int n = 0;
    for (const auto& t : tracks) {
        if (t.type != vp::TrackType::Audio) continue;
        add_radio(menu, kIdAudioBase + n, track_label(t, n + 1), t.selected);
        ++n;
    }
    if (n == 0) AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"No audio tracks");
}

void fill_speed(HMENU menu, double current) {
    for (UINT i = 0; i < std::size(kSpeeds); ++i) {
        wchar_t text[16];
        std::swprintf(text, 16, L"%g×", kSpeeds[i]);
        add_radio(menu, kIdSpeedBase + i, text, std::fabs(kSpeeds[i] - current) < 0.01);
    }
}

int64_t nth_track_id(const std::vector<vp::Track>& tracks, vp::TrackType type, UINT n) {
    for (const auto& t : tracks)
        if (t.type == type && n-- == 0) return t.id;
    return 0;
}

}  // namespace

void show_menu(AppWindow& app, MenuKind kind, POINT screen_pt, bool above) {
    apply_menu_theme(app.theme());
    const auto tracks = app.player().tracks();  // copy: the list may change while the menu is open
    const vp::PlayerState state = app.player().state();

    HMENU menu = CreatePopupMenu();
    switch (kind) {
        case MenuKind::Subtitles: fill_subtitles(menu, tracks); break;
        case MenuKind::Audio: fill_audio(menu, tracks); break;
        case MenuKind::Speed: fill_speed(menu, state.speed); break;
        case MenuKind::Full: {
            HMENU subs = CreatePopupMenu(), audio = CreatePopupMenu(), speed = CreatePopupMenu(),
                  controls = CreatePopupMenu(), startup = CreatePopupMenu();
            fill_subtitles(subs, tracks);
            fill_audio(audio, tracks);
            fill_speed(speed, state.speed);
            add_radio(controls, kIdLayoutFloating, L"Floating", app.settings().layout == vp::ControlLayout::Floating);
            add_radio(controls, kIdLayoutDocked, L"Docked", app.settings().layout == vp::ControlLayout::Docked);

            bool remember = app.settings().startup == vp::StartupMode::RememberLast;
            add_radio(startup, kIdStartupRemember, L"Remember last settings", remember);
            add_radio(startup, kIdStartupDefaults, L"Always start with defaults", !remember);

            UINT file_flags = state.has_file ? MF_STRING : MF_STRING | MF_GRAYED;
            AppendMenuW(menu, MF_STRING, kIdOpen, L"Open file…\tCtrl+O");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, file_flags, kIdPlayPause, state.paused ? L"Play\tSpace" : L"Pause\tSpace");
            AppendMenuW(menu, file_flags | MF_POPUP, reinterpret_cast<UINT_PTR>(subs), L"Subtitles");
            AppendMenuW(menu, file_flags | MF_POPUP, reinterpret_cast<UINT_PTR>(audio), L"Audio track");
            AppendMenuW(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(speed), L"Speed");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(controls), L"Controls\tL");
            AppendMenuW(menu, MF_STRING | (app.is_borderless() ? MF_CHECKED : 0), kIdBorderless, L"Borderless\tB");
            AppendMenuW(menu, MF_STRING | (app.is_fullscreen() ? MF_CHECKED : 0), kIdFullscreen, L"Fullscreen\tF");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(startup), L"Settings");
            break;
        }
    }

    app.set_menu_open(true);
    SetForegroundWindow(app.hwnd());  // required so the menu closes when the user clicks elsewhere
    UINT flags = TPM_RETURNCMD | TPM_RIGHTBUTTON | (above ? TPM_BOTTOMALIGN : TPM_TOPALIGN);
    UINT id = static_cast<UINT>(TrackPopupMenu(menu, flags, screen_pt.x, screen_pt.y, 0, app.hwnd(), nullptr));
    DestroyMenu(menu);
    app.set_menu_open(false);

    if (id == 0) return;
    if (id >= kIdSpeedBase && id < kIdSpeedBase + std::size(kSpeeds)) {
        app.set_speed(kSpeeds[id - kIdSpeedBase]);
    } else if (id >= kIdAudioBase && id < kIdSpeedBase) {
        if (int64_t track = nth_track_id(tracks, vp::TrackType::Audio, id - kIdAudioBase)) app.player().select_audio(track);
    } else if (id >= kIdSubtitleBase && id < kIdAudioBase) {
        if (int64_t track = nth_track_id(tracks, vp::TrackType::Subtitle, id - kIdSubtitleBase))
            app.player().select_subtitle(track);
    } else {
        switch (id) {
            case kIdSubtitleOff: app.player().select_subtitle(0); break;
            case kIdOpen: app.open_file_dialog(); break;
            case kIdPlayPause: app.player().toggle_pause(); break;
            case kIdBorderless: app.set_borderless(!app.is_borderless()); break;
            case kIdFullscreen: app.toggle_fullscreen(); break;
            case kIdLayoutFloating: app.set_layout(vp::ControlLayout::Floating); break;
            case kIdLayoutDocked: app.set_layout(vp::ControlLayout::Docked); break;
            case kIdStartupRemember: app.set_startup_mode(vp::StartupMode::RememberLast); break;
            case kIdStartupDefaults: app.set_startup_mode(vp::StartupMode::Defaults); break;
            default: break;
        }
    }
}

}  // namespace win
