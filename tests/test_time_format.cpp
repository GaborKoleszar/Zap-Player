#include <doctest.h>
#include <cmath>
#include <vp/time_format.h>

TEST_CASE("format_time") {
    CHECK(vp::format_time(7) == "0:07");
    CHECK(vp::format_time(1450) == "24:10");
    CHECK(vp::format_time(3822) == "1:03:42");
    CHECK(vp::format_time(59.9) == "0:59");
    CHECK(vp::format_time(-3) == "0:00");
    CHECK(vp::format_time(std::nan("")) == "0:00");
}
