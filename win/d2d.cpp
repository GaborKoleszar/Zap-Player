#include "d2d.h"

namespace win {

ID2D1Factory* d2d_factory() {
    static ID2D1Factory* factory = [] {
        ID2D1Factory* f = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &f);
        return f;
    }();
    return factory;
}

IDWriteFactory* dwrite_factory() {
    static IDWriteFactory* factory = [] {
        IDWriteFactory* f = nullptr;
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&f));
        return f;
    }();
    return factory;
}

bool ensure_window_target(HWND host, UINT dpi, ID2D1HwndRenderTarget*& target, ID2D1SolidColorBrush*& brush) {
    if (target && brush) return true;
    if (!d2d_factory()) return false;
    release(brush);
    release(target);
    RECT rc;
    GetClientRect(host, &rc);
    auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
    if (FAILED(d2d_factory()->CreateHwndRenderTarget(
            props, D2D1::HwndRenderTargetProperties(host, D2D1::SizeU(rc.right, rc.bottom)), &target)))
        return false;
    target->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    target->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &brush);
    return brush != nullptr;
}

void resize_window_target(ID2D1HwndRenderTarget* target, HWND host) {
    if (!target) return;
    RECT rc;
    GetClientRect(host, &rc);
    target->Resize(D2D1::SizeU(rc.right, rc.bottom));
}

}  // namespace win
