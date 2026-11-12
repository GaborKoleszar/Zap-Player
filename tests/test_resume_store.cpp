#include <doctest.h>
#include <string>
#include <string_view>
#include <vp/resume_store.h>
#include "test_util.h"

using namespace std::string_view_literals;

TEST_CASE("should_resume") {
    CHECK_FALSE(vp::should_resume(3, 600));
    CHECK(vp::should_resume(120, 600));
    CHECK_FALSE(vp::should_resume(595, 600));
    CHECK_FALSE(vp::should_resume(120, 0));  // unknown duration
}
TEST_CASE("resume_key is stable across spelling of the same path") {
    CHECK(vp::resume_key("C:/Videos/Sub/../A.MKV") == vp::resume_key("c:\\videos\\a.mkv"));
    CHECK(vp::resume_key(u8_as_string(u8"D:\\Árvíztűrő.mkv")) == vp::resume_key(u8_as_string(u8"d:/Árvíztűrő.mkv")));
    CHECK(vp::resume_key("c:\\videos\\a.mkv") != vp::resume_key("c:\\videos\\b.mkv"));
}
TEST_CASE("store round trip, clear, and persistence") {
    auto f = temp_dir() / "sub" / "r.ini";
    {
        vp::ResumeStore s(f);
        s.set("a", 12.5);
        s.set("b", 99);
        s.clear("b");
        REQUIRE(s.save());
    }
    vp::ResumeStore s(f);
    CHECK(s.get("a") == doctest::Approx(12.5));
    CHECK_FALSE(s.get("b").has_value());
}
TEST_CASE("store caps at 500 entries, dropping the least recently set") {
    vp::ResumeStore s(temp_dir() / "cap.ini");
    s.set("k0", 1);
    s.set("k1", 2);
    s.set("k0", 3);  // k0 is now newer than k1
    for (int i = 2; i < 501; ++i) s.set("k" + std::to_string(i), i + 1.0);
    CHECK_FALSE(s.get("k1").has_value());
    CHECK(s.get("k0") == doctest::Approx(3));
    CHECK(s.get("k500").has_value());
}
TEST_CASE("corrupt store file loads the good lines, never throws") {
    auto f = temp_dir() / "bad.ini";
    write_file(f, "abc\tx\n12\n\t\n30\tgood\n\x00\xff\n"sv);
    vp::ResumeStore s(f);
    CHECK(s.get("good") == doctest::Approx(30));
    CHECK_FALSE(s.get("x").has_value());
}
