#include "floating_panel.h"

#include <dwmapi.h>

#include <algorithm>

#include "app_window.h"

namespace win {
namespace {

constexpr wchar_t kPanelClass[] = L"ZapPanel";
constexpr UINT_PTR kFadeTimer = 1;
constexpr int kFadeStep = 255 / 9;  // about 150 ms at one step per 16 ms

}  // namespace

FloatingPanel::FloatingPanel(AppWindow& app) : app_(app), bar_(app, vp::BarStyle::Floating, true) {}

FloatingPanel::~FloatingPanel() {
    bar_.discard();
    if (hwnd_) DestroyWindow(hwnd_);
}

bool FloatingPanel::create(HINSTANCE instance) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = Proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kPanelClass;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED, kPanelClass, L"", WS_POPUP,
                            0, 0, 10, 10, app_.hwnd(), nullptr, instance, this);
    if (!hwnd_) return false;
    SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
    apply_backdrop();
    bar_.attach(hwnd_);
    return true;
}

void FloatingPanel::apply_backdrop() {
    // Acrylic behind the whole client area, rounded corners, dark or light to match the theme.
    BOOL dark = app_.theme().dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    int corner = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd_, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &corner, sizeof(corner));
    MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);

    // The DWM system backdrops (Mica, acrylic) fall back to a flat colour on a window that is
    // not active, and this panel never is. The accent policy keeps blurring what is behind the
    // window regardless, so the video shows through like frosted glass.
    struct AccentPolicy { int state; int flags; DWORD gradient_color; int animation_id; };
    struct CompositionAttributeData { int attribute; void* data; SIZE_T size; };
    using SetCompositionAttributeFn = BOOL(WINAPI*)(HWND, CompositionAttributeData*);
    static auto set_attribute = reinterpret_cast<SetCompositionAttributeFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    bool blurred = false;
    if (set_attribute) {
        const D2D1_COLOR_F& c = app_.theme().panel_fill;
        auto byte = [](float v) { return static_cast<DWORD>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
        AccentPolicy policy{4 /* ACCENT_ENABLE_ACRYLICBLURBEHIND */, 0,
                            byte(c.a) << 24 | byte(c.b) << 16 | byte(c.g) << 8 | byte(c.r), 0};
        CompositionAttributeData data{19 /* WCA_ACCENT_POLICY */, &policy, sizeof(policy)};
        blurred = set_attribute(hwnd_, &data) != 0;
    }
    // The blur carries the tint itself; without it the bar paints the tint over a plain backdrop.
    bar_.set_tinted(!blurred);
    if (!blurred) {
        int backdrop = 3;  // DWMSBT_TRANSIENTWINDOW (acrylic)
        DwmSetWindowAttribute(hwnd_, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &backdrop, sizeof(backdrop));
    }
}

void FloatingPanel::theme_changed() {
    apply_backdrop();
    bar_.invalidate();
}

void FloatingPanel::dpi_changed() {
    bar_.discard();
    reposition();
    bar_.invalidate();
}

void FloatingPanel::reposition() {
    RECT video = app_.video_rect_screen();
    int dpi = static_cast<int>(app_.dpi());
    int video_w = video.right - video.left;
    int w = std::min(MulDiv(720, dpi, 96), video_w * 8 / 10);
    int h = MulDiv(static_cast<int>(vp::kFloatingBarHeight), dpi, 96);
    int x = video.left + (video_w - w) / 2;
    int y = video.bottom - MulDiv(16, dpi, 96) - h;

    RECT now;
    GetWindowRect(hwnd_, &now);
    if (now.left == x && now.top == y && now.right - now.left == w && now.bottom - now.top == h) return;
    SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void FloatingPanel::set_visible(bool visible) {
    if (visible == want_visible_) return;
    want_visible_ = visible;
    if (visible && !IsWindowVisible(hwnd_)) {
        reposition();
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    }
    SetTimer(hwnd_, kFadeTimer, 16, nullptr);
}

bool FloatingPanel::mouse_over() const {
    if (!hwnd_ || !IsWindowVisible(hwnd_)) return false;
    POINT pt;
    RECT rc;
    GetCursorPos(&pt);
    GetWindowRect(hwnd_, &rc);
    return PtInRect(&rc, pt) != 0;
}

LRESULT CALLBACK FloatingPanel::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        static_cast<FloatingPanel*>(cs->lpCreateParams)->hwnd_ = hwnd;
    }
    auto* self = reinterpret_cast<FloatingPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT FloatingPanel::handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            bar_.paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            bar_.resize();
            return 0;
        case WM_CLOSE:
            // The panel goes only with its owner. Closing it on its own (taskkill sends WM_CLOSE
            // to every top-level window) would leave the app without controls.
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_TIMER:
            if (wp == kFadeTimer) {
                alpha_ = std::clamp(alpha_ + (want_visible_ ? kFadeStep : -kFadeStep), 0, 255);
                SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(alpha_), LWA_ALPHA);
                if (alpha_ == 255 || alpha_ == 0) {
                    KillTimer(hwnd_, kFadeTimer);
                    if (alpha_ == 0) ShowWindow(hwnd_, SW_HIDE);
                }
            }
            return 0;
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONUP:
            app_.on_user_activity();
            bar_.handle_mouse(msg, wp, lp);
            return 0;
        case WM_MOUSELEAVE:
        case WM_CAPTURECHANGED:
            bar_.handle_mouse(msg, wp, lp);
            return 0;
        case WM_MOUSEWHEEL:
            return SendMessageW(app_.hwnd(), msg, wp, lp);
        default:
            return DefWindowProcW(hwnd_, msg, wp, lp);
    }
}

}  // namespace win
