#include "vp/time_format.h"

#include <cmath>
#include <cstdio>

namespace vp {

std::string format_time(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) seconds = 0;
    auto total = static_cast<long long>(seconds);
    long long h = total / 3600, m = (total / 60) % 60, s = total % 60;
    char buf[32];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%lld:%02lld:%02lld", h, m, s);
    else
        std::snprintf(buf, sizeof(buf), "%lld:%02lld", m, s);
    return buf;
}

}  // namespace vp
