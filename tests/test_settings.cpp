#include <doctest.h>
#include <string_view>
#include <vp/settings.h>
#include "test_util.h"

using namespace std::string_view_literals;

TEST_CASE("settings round trip") {
    auto f = temp_dir() / "sub" / "s.ini";  // parent directory does not exist yet
    vp::Settings s;
    s.layout = vp::ControlLayout::Docked;
    s.volume = 35;
    s.borderless = true;
    s.window = vp::Rect{10, 20, 900, 500};
    REQUIRE(vp::save_settings(f, s));
    auto t = vp::load_settings(f);
    CHECK(t.layout == vp::ControlLayout::Docked);
    CHECK(t.volume == 35);
    CHECK(t.borderless);
    REQUIRE(t.window.has_value());
    CHECK(t.window->x == 10);
    CHECK(t.window->w == 900);
}
TEST_CASE("settings: missing file gives defaults") {
    auto t = vp::load_settings(temp_dir() / "nope.ini");
    CHECK(t.volume == 100);
    CHECK(t.layout == vp::ControlLayout::Floating);
    CHECK_FALSE(t.borderless);
    CHECK_FALSE(t.window.has_value());
}
TEST_CASE("settings: corrupt file keeps good lines, ignores bad ones") {
    auto f = temp_dir() / "bad.ini";
    write_file(f, "volume=900\nlayout=sideways\nwindow=1,2\n\x00\xff garbage\nborderless=1\n"sv);
    auto t = vp::load_settings(f);
    CHECK(t.volume == 100);
    CHECK(t.layout == vp::ControlLayout::Floating);
    CHECK_FALSE(t.window.has_value());
    CHECK(t.borderless);
}
TEST_CASE("settings: negative window coordinates survive, empty sizes do not") {
    auto f = temp_dir() / "w.ini";
    write_file(f, "window=-1200,-50,800,600\n"sv);
    CHECK(vp::load_settings(f).window->x == -1200);
    write_file(f, "window=10,10,0,600\n"sv);
    CHECK_FALSE(vp::load_settings(f).window.has_value());
}
TEST_CASE("settings remember a maximised window") {
    auto f = temp_dir() / "m.ini";
    vp::Settings s;
    CHECK_FALSE(s.maximized);
    s.maximized = true;
    REQUIRE(vp::save_settings(f, s));
    CHECK(vp::load_settings(f).maximized);
    write_file(f, "maximized=banana\n"sv);
    CHECK_FALSE(vp::load_settings(f).maximized);
}
TEST_CASE("settings: startup mode round trips and defaults to remembering") {
    auto f = temp_dir() / "st.ini";
    CHECK(vp::load_settings(temp_dir() / "nope.ini").startup == vp::StartupMode::RememberLast);
    vp::Settings s;
    s.startup = vp::StartupMode::Defaults;
    REQUIRE(vp::save_settings(f, s));
    CHECK(vp::load_settings(f).startup == vp::StartupMode::Defaults);
    write_file(f, "startup=banana\n"sv);
    CHECK(vp::load_settings(f).startup == vp::StartupMode::RememberLast);
}
TEST_CASE("startup settings: remembered as saved, or reset to defaults") {
    vp::Settings saved;
    saved.layout = vp::ControlLayout::Docked;
    saved.volume = 35;
    saved.borderless = true;
    saved.maximized = true;
    saved.window = vp::Rect{10, 20, 900, 500};

    auto kept = vp::startup_settings(saved);
    CHECK(kept.volume == 35);
    CHECK(kept.layout == vp::ControlLayout::Docked);
    CHECK(kept.window.has_value());

    saved.startup = vp::StartupMode::Defaults;
    auto fresh = vp::startup_settings(saved);
    CHECK(fresh.startup == vp::StartupMode::Defaults);  // the choice itself survives
    CHECK(fresh.volume == 100);
    CHECK(fresh.layout == vp::ControlLayout::Floating);
    CHECK_FALSE(fresh.borderless);
    CHECK_FALSE(fresh.maximized);
    CHECK_FALSE(fresh.window.has_value());
}
