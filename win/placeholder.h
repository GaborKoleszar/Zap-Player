#pragma once
#include <windows.h>

#include <string>

#include "d2d.h"

namespace win {

class AppWindow;

// What the video area shows when there is no picture: the "Open file" button, a hint, and a
// message such as "Can't play this file". Drawn with Direct2D on a transparent background so
// the window's Mica backdrop shows through.
class Placeholder {
public:
    explicit Placeholder(AppWindow& app) : app_(app) {}
    ~Placeholder();

    void attach(HWND host) { host_ = host; }
    // `button` is the Open-file button in client pixels; an empty rect means no button.
    void paint(const std::wstring& message, const RECT& button);
    void resize();
    void discard();

private:
    bool ensure_resources();

    AppWindow& app_;
    HWND host_ = nullptr;
    ID2D1HwndRenderTarget* target_ = nullptr;
    ID2D1SolidColorBrush* brush_ = nullptr;
    IDWriteTextFormat* format_ = nullptr;
};

}  // namespace win
