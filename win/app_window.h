#pragma once
#include <windows.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include <vp/player.h>
#include <vp/resume_store.h>
#include <vp/settings.h>

#include "theme.h"

namespace win {

class ControlBar;
class FloatingPanel;
class Placeholder;

inline constexpr UINT WM_APP_PLAYER_WAKEUP = WM_APP + 1;
inline constexpr UINT WM_APP_UI_READY = WM_APP + 2;

// The main window: owns the player, the video area and both control layouts.
class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    // Creates the window without showing it.
    bool create(HINSTANCE instance, const vp::Settings& settings, bool bench);
    // Creates the mpv player rendering into the video area.
    void start_player();
    // Puts the window on screen. Showing a window takes tens of milliseconds, so this is called
    // after the player has been told to open the file: the two then run side by side.
    void show();

    HWND hwnd() const { return hwnd_; }
    vp::Player& player() { return *player_; }
    vp::Settings& settings() { return settings_; }
    const Theme& theme() const { return theme_; }
    UINT dpi() const { return dpi_; }

    void open_file(const std::string& path_utf8);
    void open_file_dialog();
    void show_message(std::wstring text);

    void toggle_fullscreen();
    bool is_fullscreen() const { return fullscreen_; }
    void set_borderless(bool borderless);
    bool is_borderless() const { return borderless_; }
    void set_layout(vp::ControlLayout layout);
    // Chooses what the next start begins with; written to disk straight away.
    void set_startup_mode(vp::StartupMode mode);
    void set_speed(double speed);
    void cycle_speed(int direction);
    void cycle_audio();
    void cycle_subtitle();
    void change_volume(int delta);

    // Screen rectangle of the video area (where the floating panel sits).
    RECT video_rect_screen() const;
    // Any mouse or keyboard activity: shows the overlay controls and restarts the idle timer.
    void on_user_activity();
    // Menus keep the overlay controls from hiding while they are open.
    void set_menu_open(bool open);

private:
    static LRESULT CALLBACK MainProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK VideoProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK StripProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle_strip(HWND strip, UINT msg, WPARAM wp, LPARAM lp);

    void on_player_wakeup();
    void on_ui_ready();
    void layout_children();
    bool strip_visible() const;
    bool overlay_wanted() const;
    void update_overlay();
    void invalidate_controls();
    void paint_video_placeholder(HWND video);
    RECT open_button_rect() const;
    bool on_key(WPARAM vk, bool repeat);
    LRESULT hit_test_borderless(LPARAM lp) const;
    void remember_position();
    void autosave_position();
    void save_state();
    void update_title();
    void disable_video_children();
    int scale(int dip) const { return MulDiv(dip, static_cast<int>(dpi_), 96); }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND video_ = nullptr;
    HWND strip_ = nullptr;
    UINT dpi_ = 96;
    Theme theme_;
    vp::Settings settings_;
    bool bench_ = false;
    bool bench_reported_ = false;

    std::unique_ptr<vp::Player> player_;
    std::optional<vp::ResumeStore> resume_;
    std::string current_path_;   // UTF-8; empty when nothing is loaded
    std::wstring message_;       // shown in the video area when there is no picture

    bool ui_ready_ = false;      // false until the first frame: keeps UI work off the startup path
    std::unique_ptr<ControlBar> docked_bar_;
    std::unique_ptr<FloatingPanel> panel_;
    std::unique_ptr<Placeholder> placeholder_;

    bool fullscreen_ = false;
    bool borderless_ = false;
    WINDOWPLACEMENT saved_placement_{sizeof(WINDOWPLACEMENT)};

    bool pressed_ = false;       // left button went down on the video
    POINT press_point_{};
    POINT last_mouse_{-1, -1};
    ULONGLONG last_activity_ = 0;
    bool cursor_hidden_ = false;
    double subtitle_lift_ = 0;   // fraction of the video height subtitles are currently raised by
    bool menu_open_ = false;
    bool closing_ = false;       // WM_CLOSE has started
    ULONGLONG last_autosave_ = 0;
    double autosaved_position_ = -1;
    std::atomic<bool> wakeup_pending_{false};
};

}  // namespace win
