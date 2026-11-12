#include <doctest.h>
#include <vp/command_line.h>
#include "test_util.h"

TEST_CASE("parse_command_line") {
    CHECK_FALSE(vp::parse_command_line({}).file_utf8.has_value());
    auto a = vp::parse_command_line({"C:\\v\\a b.mkv"});
    CHECK(a.file_utf8 == "C:\\v\\a b.mkv");
    CHECK_FALSE(a.bench);
    auto b = vp::parse_command_line({"--bench", "x.mp4"});
    CHECK(b.bench);
    CHECK(b.file_utf8 == "x.mp4");
    auto name = u8_as_string(u8"D:\\Árvíztűrő tükörfúrógép.mkv");
    CHECK(vp::parse_command_line({name}).file_utf8 == name);  // bytes pass through untouched
    CHECK(vp::parse_command_line({"a.mkv", "b.mkv"}).file_utf8 == "a.mkv");  // first file wins
    CHECK_FALSE(vp::parse_command_line({"--unknown"}).file_utf8.has_value());  // unknown switches ignored
}
