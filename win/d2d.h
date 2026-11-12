#pragma once
#include <d2d1.h>
#include <dwrite.h>

namespace win {

// Process-wide Direct2D / DirectWrite factories, created on first use (never before the first
// video frame: they are not needed to start playback).
ID2D1Factory* d2d_factory();
IDWriteFactory* dwrite_factory();

// Creates the Direct2D target that draws into `host`, plus one brush, if `target` is still null.
// Transparent pixels let the backdrop behind the window show, so text uses greyscale
// antialiasing (ClearType needs an opaque background). Returns false if it cannot be created.
bool ensure_window_target(HWND host, UINT dpi, ID2D1HwndRenderTarget*& target, ID2D1SolidColorBrush*& brush);
// Matches the target to the host's client size; call from WM_SIZE. A null target is fine.
void resize_window_target(ID2D1HwndRenderTarget* target, HWND host);

template <typename T>
void release(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

}  // namespace win
