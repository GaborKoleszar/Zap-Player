#include "placeholder.h"

#include "app_window.h"

namespace win {

Placeholder::~Placeholder() {
    discard();
    release(format_);
}

void Placeholder::discard() {
    release(brush_);
    release(target_);
}

void Placeholder::resize() { resize_window_target(target_, host_); }

bool Placeholder::ensure_resources() {
    if (!dwrite_factory() || !ensure_window_target(host_, app_.dpi(), target_, brush_)) return false;
    if (!format_) {
        dwrite_factory()->CreateTextFormat(L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &format_);
        if (format_) {
            format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }
    return target_ && brush_ && format_;
}

void Placeholder::paint(const std::wstring& message, const RECT& button) {
    PAINTSTRUCT ps;
    BeginPaint(host_, &ps);
    EndPaint(host_, &ps);
    if (!ensure_resources()) return;

    const Theme& t = app_.theme();
    const float scale = 96.0f / static_cast<float>(app_.dpi());
    const D2D1_SIZE_F size = target_->GetSize();
    auto draw_text = [&](const wchar_t* text, D2D1_RECT_F rect, const D2D1_COLOR_F& color) {
        brush_->SetColor(color);
        target_->DrawText(text, static_cast<UINT32>(wcslen(text)), format_, rect, brush_);
    };

    target_->BeginDraw();
    target_->Clear(D2D1::ColorF(0, 0, 0, 0));  // fully transparent: the Mica backdrop shows through

    const bool has_button = button.right > button.left;
    if (has_button) {
        D2D1_RECT_F b{button.left * scale, button.top * scale, button.right * scale, button.bottom * scale};
        brush_->SetColor(t.hover_fill);
        target_->FillRoundedRectangle(D2D1::RoundedRect(b, 5, 5), brush_);
        brush_->SetColor(t.track_fill);
        target_->DrawRoundedRectangle(D2D1::RoundedRect(b, 5, 5), brush_, 1.0f);
        draw_text(L"Open file", b, t.text);
        draw_text(L"or drop a video here", D2D1::RectF(0, b.bottom + 8, size.width, b.bottom + 34), t.text_dim);
        if (!message.empty())
            draw_text(message.c_str(), D2D1::RectF(0, b.top - 44, size.width, b.top - 12), t.text);
    } else if (!message.empty()) {
        draw_text(message.c_str(), D2D1::RectF(0, 0, size.width, size.height), t.text);
    }

    if (target_->EndDraw() == D2DERR_RECREATE_TARGET) discard();
}

}  // namespace win
