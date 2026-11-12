#pragma once
#include <optional>
#include <span>

namespace vp {

struct Rect { int x, y, w, h; };
struct Size { int w, h; };

// Returns `saved` if at least 100x100 px of it lies inside one work area; otherwise the
// fallback size centred in the first work area, shrunk to fit it.
Rect fit_rect_to_work_areas(std::optional<Rect> saved, std::span<const Rect> work_areas, Size fallback);

}  // namespace vp
