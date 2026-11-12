#pragma once
#include <string>

namespace vp {
// "0:07", "24:10", "1:03:42". Negative or non-finite input gives "0:00".
std::string format_time(double seconds);
}
