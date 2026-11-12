#include "app_window.h"

#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "bench.h"
#include "context_menu.h"
#include "control_bar.h"
#include "floating_panel.h"
#include "paths.h"
#include "placeholder.h"
#include "single_instance.h"

namespace win {
namespace {

constexpr wchar_t kVideoClass[] = L"ZapVideo";
constexpr wchar_t kStripClass[] = L"ZapStrip";
constexpr UINT_PTR kIdleTimer = 1;
constexpr ULONGLONG kIdleHideMs = 2000;
constexpr ULONGLONG kAutosaveMs = 5000;
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

AppWindow* app_from(HWND hwnd) { return reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)); }

std::vector<vp::Rect> monitor_work_areas() {
    std::vector<vp::Rect> areas;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM out) -> BOOL {
        MONITORINFO mi{sizeof(mi)};
        if (GetMonitorInfoW(monitor, &mi)) {
            vp::Rect r{mi.rcWork.left, mi.rcWork.top, mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top};
            auto* list = reinterpret_cast<std::vector<vp::Rect>*>(out);
            // The primary monitor goes first: it is the fallback position.
            if (mi.dwFlags & MONITORINFOF_PRIMARY) list->insert(list->begin(), r);
            else list->push_back(r);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&areas));
    return areas;
}

}  // namespace

AppWindow::AppWindow() = default;
AppWindow::~AppWindow() = default;

bool AppWindow::create(HINSTANCE instance, const vp::Settings& settings, bool bench) {
    instance_ = instance;
    settings_ = settings;
    bench_ = bench;
    borderless_ = settings.borderless;
    theme_ = current_theme();

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = MainProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kMainWindowClass;
    RegisterClassExW(&wc);

    WNDCLASSEXW vc{sizeof(vc)};
    vc.lpfnWndProc = VideoProc;
    vc.hInstance = instance;
    vc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    vc.lpszClassName = kVideoClass;
    RegisterClassExW(&vc);

    WNDCLASSEXW sc{sizeof(sc)};
    sc.lpfnWndProc = StripProc;
    sc.hInstance = instance;
    sc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    sc.lpszClassName = kStripClass;
    RegisterClassExW(&sc);

    int system_dpi = static_cast<int>(GetDpiForSystem());
    vp::Size fallback{MulDiv(1280, system_dpi, 96), MulDiv(720, system_dpi, 96)};
    auto areas = monitor_work_areas();
    vp::Rect r = vp::fit_rect_to_work_areas(settings.window, areas, fallback);

    hwnd_ = CreateWindowExW(WS_EX_ACCEPTFILES, kMainWindowClass, L"Zap", kWindowedStyle, r.x, r.y, r.w, r.h,
                            nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    dpi_ = GetDpiForWindow(hwnd_);
    apply_title_bar_theme(hwnd_, theme_);
    apply_mica(hwnd_);

    // Disabled so that mouse input over the video reaches the main window instead of mpv's window.
    video_ = CreateWindowExW(0, kVideoClass, L"", WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPCHILDREN, 0, 0, 10, 10,
                             hwnd_, nullptr, instance, nullptr);
    if (settings_.layout == vp::ControlLayout::Docked) set_layout(vp::ControlLayout::Docked);
    if (borderless_) SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    layout_children();
    return true;
}

void AppWindow::show() {
    ShowWindow(hwnd_, settings_.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(hwnd_);
}

void AppWindow::start_player() {
    vp::PlayerOptions options;
    options.wid = static_cast<int64_t>(reinterpret_cast<intptr_t>(video_));
    options.volume = settings_.volume;
    // Forward slashes: mpv only creates missing parent folders along "/" separators.
    options.cache_dir = narrow(cache_dir().generic_wstring());
    wchar_t log[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"VP_MPV_LOG", log, MAX_PATH);  // diagnostics: set to a file path
    if (n > 0 && n < MAX_PATH) options.log_file = narrow(log);
    player_ = std::make_unique<vp::Player>(options);
    player_->set_wakeup([this] {
        // Called on an mpv thread. One pending message is enough: poll() drains everything.
        if (!wakeup_pending_.exchange(true)) PostMessageW(hwnd_, WM_APP_PLAYER_WAKEUP, 0, 0);
    });
}

// ---------------------------------------------------------------------------------------------
// Files

void AppWindow::open_file(const std::string& path_utf8) {
    if (!player_ || closing_) return;  // can be reached from a dialog or hand-over during shutdown
    remember_position();
    double start = 0;
    if (!bench_) {
        if (!resume_) resume_.emplace(app_data_dir() / L"resume.ini");
        start = resume_->get(vp::resume_key(path_utf8)).value_or(0);
    }
    current_path_ = path_utf8;
    message_.clear();
    InvalidateRect(video_, nullptr, TRUE);
    player_->load(path_utf8, start);
    update_title();
    on_user_activity();
}

void AppWindow::open_file_dialog() {
    std::wstring buffer(32768, L'\0');
    OPENFILENAMEW ofn{sizeof(ofn)};
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.lpstrFile = buffer.data();
    ofn.nMaxFile = static_cast<DWORD>(buffer.size());
    // OFN_NOCHANGEDIR: otherwise the folder browsed last becomes the working directory and stays
    // locked (cannot be renamed, drive cannot be ejected) for as long as the player runs.
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR;
    set_menu_open(true);
    bool chosen = GetOpenFileNameW(&ofn) != 0;
    set_menu_open(false);
    if (chosen) open_file(narrow(buffer.c_str()));
}

void AppWindow::show_message(std::wstring text) {
    message_ = std::move(text);
    InvalidateRect(video_, nullptr, TRUE);
}

void AppWindow::remember_position() {
    if (bench_ || current_path_.empty() || !resume_ || !player_) return;
    const vp::PlayerState& s = player_->state();
    if (!s.has_file) return;
    std::string key = vp::resume_key(current_path_);
    if (!s.at_end && vp::should_resume(s.position, s.duration)) resume_->set(key, s.position);
    else resume_->clear(key);
    resume_->save();
}

void AppWindow::autosave_position() {
    // The position is also written while playing, so that a crash, a kill or a power cut
    // loses at most a few seconds.
    if (bench_ || !player_ || current_path_.empty()) return;
    ULONGLONG now = GetTickCount64();
    if (now - last_autosave_ < kAutosaveMs) return;
    last_autosave_ = now;
    double position = player_->state().position;
    if (std::fabs(position - autosaved_position_) < 1.0) return;  // paused or barely moved
    autosaved_position_ = position;
    remember_position();
}

void AppWindow::save_state() {
    if (bench_) return;
    if (player_) {
        player_->poll();
        remember_position();
        settings_.volume = player_->state().volume;
    }
    settings_.borderless = borderless_;
    WINDOWPLACEMENT placement{sizeof(placement)};
    if (fullscreen_) placement = saved_placement_;
    else GetWindowPlacement(hwnd_, &placement);
    // A minimised window that will restore to maximised counts as maximised.
    settings_.maximized = placement.showCmd == SW_SHOWMAXIMIZED ||
                          (placement.showCmd == SW_SHOWMINIMIZED && (placement.flags & WPF_RESTORETOMAXIMIZED));
    const RECT& n = placement.rcNormalPosition;
    if (n.right > n.left && n.bottom > n.top) settings_.window = vp::Rect{n.left, n.top, n.right - n.left, n.bottom - n.top};
    vp::save_settings(app_data_dir() / L"settings.ini", settings_);
}

void AppWindow::set_startup_mode(vp::StartupMode mode) {
    settings_.startup = mode;
    save_state();
}

void AppWindow::update_title() {
    std::wstring title = L"Zap";
    if (!current_path_.empty()) {
        std::wstring path = widen(current_path_);
        size_t slash = path.find_last_of(L"\\/");
        title = path.substr(slash == std::wstring::npos ? 0 : slash + 1) + L" - Zap";
    }
    SetWindowTextW(hwnd_, title.c_str());
}

// ---------------------------------------------------------------------------------------------
// Player events

void AppWindow::disable_video_children() {
    EnumChildWindows(video_, [](HWND child, LPARAM) -> BOOL {
        EnableWindow(child, FALSE);
        return TRUE;
    }, 0);
}

void AppWindow::on_player_wakeup() {
    if (!player_) return;
    bool changed = false;
    for (vp::PlayerEvent ev : player_->poll()) {
        switch (ev) {
            case vp::PlayerEvent::FileLoaded:
                bench_trace("file loaded");
                disable_video_children();
                break;
            case vp::PlayerEvent::FirstFrame:
                bench_trace("playback started");
                disable_video_children();
                if (bench_ && !bench_reported_) {
                    bench_reported_ = true;
                    bench_report_first_frame();
                    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
                }
                if (!ui_ready_) PostMessageW(hwnd_, WM_APP_UI_READY, 0, 0);
                break;
            case vp::PlayerEvent::LoadFailed:
                current_path_.clear();
                update_title();
                show_message(L"Can't play this file");
                if (!ui_ready_) PostMessageW(hwnd_, WM_APP_UI_READY, 0, 0);
                break;
            case vp::PlayerEvent::EndOfFile:
                if (!bench_ && resume_ && !current_path_.empty()) {
                    resume_->clear(vp::resume_key(current_path_));
                    resume_->save();
                }
                break;
            case vp::PlayerEvent::StateChanged:
            case vp::PlayerEvent::TracksChanged:
                changed = true;
                break;
        }
    }
    if (changed) {
        invalidate_controls();
        update_overlay();
    }
}

void AppWindow::on_ui_ready() {
    if (ui_ready_) return;
    ui_ready_ = true;
    last_activity_ = GetTickCount64();
    SetTimer(hwnd_, kIdleTimer, 250, nullptr);
    if (strip_) InvalidateRect(strip_, nullptr, FALSE);
    InvalidateRect(video_, nullptr, FALSE);
    update_overlay();
}

// ---------------------------------------------------------------------------------------------
// Layout and overlay controls

bool AppWindow::strip_visible() const {
    return settings_.layout == vp::ControlLayout::Docked && !fullscreen_ && !borderless_;
}

bool AppWindow::overlay_wanted() const {
    return settings_.layout == vp::ControlLayout::Floating || fullscreen_ || borderless_;
}

void AppWindow::layout_children() {
    if (!video_) return;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int strip_h = (strip_ && strip_visible()) ? scale(static_cast<int>(vp::kDockedBarHeight)) : 0;
    MoveWindow(video_, 0, 0, rc.right, std::max<int>(rc.bottom - strip_h, 0), TRUE);
    if (strip_) {
        if (strip_h > 0) {
            MoveWindow(strip_, 0, rc.bottom - strip_h, rc.right, strip_h, TRUE);
            ShowWindow(strip_, SW_SHOWNA);
        } else {
            ShowWindow(strip_, SW_HIDE);
        }
    }
}

void AppWindow::set_layout(vp::ControlLayout layout) {
    settings_.layout = layout;
    if (layout == vp::ControlLayout::Docked && !strip_) {
        strip_ = CreateWindowExW(0, kStripClass, L"", WS_CHILD, 0, 0, 10, 10, hwnd_, nullptr, instance_, nullptr);
        docked_bar_ = std::make_unique<ControlBar>(*this, vp::BarStyle::Docked, false /* clear: Mica shows through */);
        docked_bar_->attach(strip_);
    }
    layout_children();
    last_activity_ = GetTickCount64();
    update_overlay();
}

void AppWindow::invalidate_controls() {
    if (docked_bar_) docked_bar_->invalidate();
    if (panel_) panel_->bar().invalidate();
}

RECT AppWindow::video_rect_screen() const {
    RECT rc{};
    GetWindowRect(video_, &rc);
    return rc;
}

void AppWindow::on_user_activity() {
    last_activity_ = GetTickCount64();
    update_overlay();
}

void AppWindow::set_menu_open(bool open) {
    menu_open_ = open;
    on_user_activity();
}

void AppWindow::update_overlay() {
    if (!ui_ready_ || !player_) return;
    bool wanted = overlay_wanted() && IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
    const vp::PlayerState& s = player_->state();
    bool idle = GetTickCount64() - last_activity_ > kIdleHideMs;
    bool keep = s.paused || !s.has_file || s.at_end || menu_open_ ||
                (panel_ && (panel_->mouse_over() || panel_->bar().dragging()));
    bool show = wanted && (!idle || keep);

    if (show && !panel_) {
        panel_ = std::make_unique<FloatingPanel>(*this);
        if (!panel_->create(instance_)) panel_.reset();
    }
    if (panel_) {
        if (show) panel_->reposition();
        panel_->set_visible(show);
    }

    // Subtitles move up while the panel is on screen so it does not cover them.
    double lift = 0;
    if (show && panel_) {
        RECT video = video_rect_screen();
        int video_h = video.bottom - video.top;
        if (video_h > 0) lift = static_cast<double>(scale(static_cast<int>(vp::kFloatingBarHeight) + 16 + 8)) / video_h;
    }
    if (std::fabs(lift - subtitle_lift_) > 0.005) {
        subtitle_lift_ = lift;
        player_->set_subtitle_lift(lift);
    }

    // With the controls gone in fullscreen or borderless, the pointer goes too.
    bool hide_cursor = (fullscreen_ || borderless_) && wanted && !show;
    if (hide_cursor != cursor_hidden_) {
        cursor_hidden_ = hide_cursor;
        POINT pt;
        GetCursorPos(&pt);
        RECT video = video_rect_screen();
        if (PtInRect(&video, pt)) SetCursor(hide_cursor ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
    }
}

// ---------------------------------------------------------------------------------------------
// Window modes

void AppWindow::toggle_fullscreen() {
    if (!fullscreen_) {
        GetWindowPlacement(hwnd_, &saved_placement_);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
        fullscreen_ = true;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN);
        // No SWP_NOOWNERZORDER here: with it the window is raised without its owned floating
        // panel, which then sits underneath the video from the second fullscreen on.
        SetWindowPos(hwnd_, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
    } else {
        fullscreen_ = false;
        SetWindowLongPtrW(hwnd_, GWL_STYLE, kWindowedStyle | WS_VISIBLE);
        SetWindowPlacement(hwnd_, &saved_placement_);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    layout_children();
    invalidate_controls();
    on_user_activity();
}

void AppWindow::set_borderless(bool borderless) {
    borderless_ = borderless;
    settings_.borderless = borderless;
    if (!fullscreen_) SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    layout_children();
    invalidate_controls();
    on_user_activity();
}

LRESULT AppWindow::hit_test_borderless(LPARAM lp) const {
    if (IsZoomed(hwnd_)) return HTCLIENT;
    POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    RECT rc;
    GetWindowRect(hwnd_, &rc);
    int b = scale(6);
    bool left = pt.x < rc.left + b, right = pt.x >= rc.right - b;
    bool top = pt.y < rc.top + b, bottom = pt.y >= rc.bottom - b;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    return HTCLIENT;
}

// ---------------------------------------------------------------------------------------------
// Playback shortcuts

void AppWindow::set_speed(double speed) { player_->set_speed(speed); }

void AppWindow::cycle_speed(int direction) {
    double current = player_->state().speed;
    int nearest = 0;
    for (int i = 0; i < static_cast<int>(std::size(kSpeeds)); ++i)
        if (std::fabs(kSpeeds[i] - current) < std::fabs(kSpeeds[nearest] - current)) nearest = i;
    int next = std::clamp(nearest + direction, 0, static_cast<int>(std::size(kSpeeds)) - 1);
    player_->set_speed(kSpeeds[next]);
}

void AppWindow::cycle_audio() {
    std::vector<int64_t> ids;
    int64_t selected = 0;
    for (const auto& t : player_->tracks()) {
        if (t.type != vp::TrackType::Audio) continue;
        ids.push_back(t.id);
        if (t.selected) selected = t.id;
    }
    if (ids.size() < 2) return;
    auto it = std::find(ids.begin(), ids.end(), selected);
    size_t next = it == ids.end() ? 0 : (static_cast<size_t>(it - ids.begin()) + 1) % ids.size();
    player_->select_audio(ids[next]);
}

void AppWindow::cycle_subtitle() {
    std::vector<int64_t> ids{0};  // 0 = off
    int64_t selected = 0;
    for (const auto& t : player_->tracks()) {
        if (t.type != vp::TrackType::Subtitle) continue;
        ids.push_back(t.id);
        if (t.selected) selected = t.id;
    }
    if (ids.size() < 2) return;
    auto it = std::find(ids.begin(), ids.end(), selected);
    size_t next = (static_cast<size_t>(it - ids.begin()) + 1) % ids.size();
    player_->select_subtitle(ids[next]);
}

void AppWindow::change_volume(int delta) {
    player_->set_volume(std::clamp(player_->state().volume + delta, 0, 100));
}

bool AppWindow::on_key(WPARAM vk, bool repeat) {
    bool ctrl = GetKeyState(VK_CONTROL) < 0;
    if (repeat) {
        // A held key keeps seeking, changing volume or stepping speed, but a toggle acts once:
        // otherwise holding F a moment too long flips fullscreen back and forth.
        switch (vk) {
            case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_ADD: case VK_SUBTRACT: break;
            default: return true;
        }
    }
    switch (vk) {
        case VK_SPACE: player_->toggle_pause(); return true;
        case VK_LEFT: player_->seek_relative(ctrl ? -60 : -5); return true;
        case VK_RIGHT: player_->seek_relative(ctrl ? 60 : 5); return true;
        case VK_UP: change_volume(5); return true;
        case VK_DOWN: change_volume(-5); return true;
        case 'M': player_->toggle_mute(); return true;
        case 'F':
        case VK_F11: toggle_fullscreen(); return true;
        case VK_ESCAPE:
            if (fullscreen_) toggle_fullscreen();
            return true;
        case 'B': set_borderless(!borderless_); return true;
        case 'L':
            set_layout(settings_.layout == vp::ControlLayout::Floating ? vp::ControlLayout::Docked
                                                                       : vp::ControlLayout::Floating);
            return true;
        case 'S': cycle_subtitle(); return true;
        case 'A': cycle_audio(); return true;
        case VK_ADD: cycle_speed(1); return true;
        case VK_SUBTRACT: cycle_speed(-1); return true;
        case 'O':
            if (ctrl) open_file_dialog();
            return true;
        default: return false;
    }
}

// ---------------------------------------------------------------------------------------------
// Painting the video area when there is no picture

RECT AppWindow::open_button_rect() const {
    RECT rc;
    GetClientRect(video_, &rc);
    int w = scale(140), h = scale(36);
    int x = (rc.right - w) / 2, y = (rc.bottom - h) / 2;
    return {x, y, x + w, y + h};
}

void AppWindow::paint_video_placeholder(HWND video) {
    if (!ui_ready_) {
        // Before the first frame nothing but a fill: black here means "transparent", so the
        // Mica backdrop shows, and Direct2D start-up stays off the start-up path.
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(video, &ps);
        FillRect(dc, &ps.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        EndPaint(video, &ps);
        return;
    }
    if (!placeholder_) {
        placeholder_ = std::make_unique<Placeholder>(*this);
        placeholder_->attach(video);
    }
    RECT button{};
    if (current_path_.empty()) button = open_button_rect();
    placeholder_->paint(message_, button);
}

// ---------------------------------------------------------------------------------------------
// Window procedures

LRESULT CALLBACK AppWindow::MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* self = static_cast<AppWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    AppWindow* self = app_from(hwnd);
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK AppWindow::VideoProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        if (AppWindow* self = app_from(GetParent(hwnd))) {
            self->paint_video_placeholder(hwnd);
            return 0;
        }
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK AppWindow::StripProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AppWindow* self = app_from(GetParent(hwnd));
    return self ? self->handle_strip(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT AppWindow::handle_strip(HWND strip, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            if (ui_ready_ && docked_bar_) {
                docked_bar_->paint();
            } else {
                // Before the first frame: a plain fill (black is transparent here, so Mica shows), so
                // that Direct2D start-up does not delay playback.
                PAINTSTRUCT ps;
                HDC dc = BeginPaint(strip, &ps);
                FillRect(dc, &ps.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                EndPaint(strip, &ps);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (docked_bar_) docked_bar_->resize();
            return 0;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_MOUSELEAVE:
        case WM_CAPTURECHANGED:
            if (ui_ready_ && docked_bar_) docked_bar_->handle_mouse(msg, wp, lp);
            return 0;
        case WM_MOUSEWHEEL:
            return SendMessageW(hwnd_, msg, wp, lp);
        default:
            return DefWindowProcW(strip, msg, wp, lp);
    }
}

LRESULT AppWindow::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_APP_PLAYER_WAKEUP:
            wakeup_pending_ = false;
            on_player_wakeup();
            return 0;
        case WM_APP_UI_READY:
            on_ui_ready();
            return 0;
        case WM_TIMER:
            if (wp == kIdleTimer) {
                update_overlay();
                autosave_position();
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            layout_children();
            if (placeholder_) placeholder_->resize();
            update_overlay();
            return 0;
        case WM_MOVE:
            update_overlay();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lp);
            info->ptMinTrackSize = {scale(360), scale(240)};
            return 0;
        }
        case WM_DPICHANGED: {
            dpi_ = HIWORD(wp);
            RECT target = *reinterpret_cast<const RECT*>(lp);
            if (fullscreen_) {
                // Fullscreen must keep covering the monitor it has just been moved to.
                MONITORINFO mi{sizeof(mi)};
                if (GetMonitorInfoW(MonitorFromRect(&target, MONITOR_DEFAULTTONEAREST), &mi)) target = mi.rcMonitor;
            }
            SetWindowPos(hwnd_, nullptr, target.left, target.top, target.right - target.left,
                         target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
            if (placeholder_) placeholder_->discard();
            if (docked_bar_) docked_bar_->discard();
            if (panel_) panel_->dpi_changed();
            layout_children();
            InvalidateRect(video_, nullptr, TRUE);
            return 0;
        }
        case WM_NCCALCSIZE:
            if (borderless_ && !fullscreen_ && wp) {
                // The whole window becomes client area: no title bar, no visible frame.
                if (IsZoomed(hwnd_)) {
                    MONITORINFO mi{sizeof(mi)};
                    GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
                    reinterpret_cast<NCCALCSIZE_PARAMS*>(lp)->rgrc[0] = mi.rcWork;
                }
                return 0;
            }
            break;
        case WM_NCHITTEST:
            if (borderless_ && !fullscreen_) return hit_test_borderless(lp);
            break;

        case WM_SETTINGCHANGE:
        case WM_DWMCOLORIZATIONCOLORCHANGED:
            if (msg == WM_DWMCOLORIZATIONCOLORCHANGED ||
                (lp && lstrcmpiW(reinterpret_cast<const wchar_t*>(lp), L"ImmersiveColorSet") == 0)) {
                theme_ = current_theme();
                apply_title_bar_theme(hwnd_, theme_);
                if (panel_) panel_->theme_changed();
                invalidate_controls();
                InvalidateRect(video_, nullptr, FALSE);
                if (strip_) InvalidateRect(strip_, nullptr, FALSE);
            }
            break;

        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wp);
            UINT len = DragQueryFileW(drop, 0, nullptr, 0);
            std::wstring path(len + 1, L'\0');
            DragQueryFileW(drop, 0, path.data(), len + 1);
            DragFinish(drop);
            path.resize(len);
            if (!path.empty()) open_file(narrow(path));
            return 0;
        }
        case WM_COPYDATA: {
            auto* data = reinterpret_cast<const COPYDATASTRUCT*>(lp);
            if (!data || data->dwData != kCopyDataOpenFile) break;
            // While shutting down, refuse: the sender then opens the file in its own window.
            if (closing_ || !player_) return FALSE;
            if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
            SetForegroundWindow(hwnd_);
            size_t chars = data->cbData / sizeof(wchar_t);
            if (data->lpData && chars > 1) {
                std::wstring path(static_cast<const wchar_t*>(data->lpData), chars - 1);
                if (size_t nul = path.find(L'\0'); nul != std::wstring::npos) path.resize(nul);
                if (!path.empty()) open_file(narrow(path));
            }
            return TRUE;
        }

        case WM_KEYDOWN:
            if (player_ && on_key(wp, (lp & (1 << 30)) != 0)) {
                on_user_activity();
                return 0;
            }
            break;
        case WM_CHAR:
            // Speed keys by character, so they work on any keyboard layout that has them.
            if (player_ && (wp == L'[' || wp == L']')) {
                cycle_speed(wp == L']' ? 1 : -1);
                on_user_activity();
                return 0;
            }
            break;

        case WM_MOUSEMOVE: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (pt.x != last_mouse_.x || pt.y != last_mouse_.y) {
                last_mouse_ = pt;
                on_user_activity();
            }
            if (pressed_ && !fullscreen_ &&
                (std::abs(pt.x - press_point_.x) > GetSystemMetrics(SM_CXDRAG) ||
                 std::abs(pt.y - press_point_.y) > GetSystemMetrics(SM_CYDRAG))) {
                // Dragging the video moves the window.
                pressed_ = false;
                ReleaseCapture();
                POINT screen = pt;
                ClientToScreen(hwnd_, &screen);
                SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(screen.x, screen.y));
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            pressed_ = true;
            press_point_ = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            SetCapture(hwnd_);
            return 0;
        case WM_LBUTTONUP:
            if (pressed_) {
                pressed_ = false;
                ReleaseCapture();
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                RECT button = open_button_rect();
                if (current_path_.empty() && PtInRect(&button, pt)) open_file_dialog();
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            pressed_ = false;
            toggle_fullscreen();
            return 0;
        case WM_CAPTURECHANGED:
            pressed_ = false;
            return 0;
        case WM_RBUTTONUP: {
            if (!player_) return 0;
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ClientToScreen(hwnd_, &pt);
            show_menu(*this, MenuKind::Full, pt);
            return 0;
        }
        case WM_MOUSEWHEEL:
            if (player_) {
                change_volume(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 5 : -5);
                on_user_activity();
            }
            return 0;
        case WM_SETCURSOR:
            if (cursor_hidden_ && LOWORD(lp) == HTCLIENT) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;

        case WM_ENDSESSION:
            // Shutdown or log-off: WM_CLOSE never arrives, so save here.
            if (wp) save_state();
            return 0;
        case WM_CLOSE:
            closing_ = true;
            ShowWindow(hwnd_, SW_HIDE);  // tearing mpv down takes a moment; do it out of sight
            save_state();
            KillTimer(hwnd_, kIdleTimer);
            panel_.reset();
            docked_bar_.reset();
            placeholder_.reset();
            if (player_) player_->set_wakeup(nullptr);
            player_.reset();
            DestroyWindow(hwnd_);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace win
