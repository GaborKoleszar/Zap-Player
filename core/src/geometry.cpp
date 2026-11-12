#include "vp/geometry.h"

#include <algorithm>

namespace vp {

Rect fit_rect_to_work_areas(std::optional<Rect> saved, std::span<const Rect> work_areas, Size fallback) {
    if (saved) {
        for (const Rect& a : work_areas) {
            int overlap_w = std::min(saved->x + saved->w, a.x + a.w) - std::max(saved->x, a.x);
            int overlap_h = std::min(saved->y + saved->h, a.y + a.h) - std::max(saved->y, a.y);
            if (overlap_w >= 100 && overlap_h >= 100) return *saved;
        }
    }
    if (work_areas.empty()) return {0, 0, fallback.w, fallback.h};
    const Rect& a = work_areas[0];
    int w = std::min(fallback.w, a.w), h = std::min(fallback.h, a.h);
    return {a.x + (a.w - w) / 2, a.y + (a.h - h) / 2, w, h};
}

}  // namespace vp
