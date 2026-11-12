#include <doctest.h>
#include <vp/geometry.h>

TEST_CASE("fit_rect keeps a visible saved rect") {
    vp::Rect wa[] = {{0, 0, 1920, 1040}};
    auto r = vp::fit_rect_to_work_areas(vp::Rect{100, 100, 800, 600}, wa, {1280, 720});
    CHECK(r.x == 100);
    CHECK(r.w == 800);
}
TEST_CASE("fit_rect keeps a rect on a second monitor") {
    vp::Rect wa[] = {{0, 0, 1920, 1040}, {1920, 0, 1920, 1040}};
    auto r = vp::fit_rect_to_work_areas(vp::Rect{2500, 100, 800, 600}, wa, {1280, 720});
    CHECK(r.x == 2500);
}
TEST_CASE("fit_rect recentres an off-screen rect") {
    vp::Rect wa[] = {{0, 0, 1920, 1040}};
    auto r = vp::fit_rect_to_work_areas(vp::Rect{5000, 100, 800, 600}, wa, {1280, 720});
    CHECK(r.w == 1280);
    CHECK(r.h == 720);
    CHECK(r.x == 320);
    CHECK(r.y == 160);
}
TEST_CASE("fit_rect shrinks fallback on a small screen and handles no saved rect") {
    vp::Rect wa[] = {{0, 0, 1024, 600}};
    auto r = vp::fit_rect_to_work_areas(std::nullopt, wa, {1280, 720});
    CHECK(r.w <= 1024);
    CHECK(r.h <= 600);
    CHECK(r.x >= 0);
    CHECK(r.y >= 0);
}
TEST_CASE("fit_rect with no work areas returns the fallback at the origin") {
    auto r = vp::fit_rect_to_work_areas(std::nullopt, {}, {1280, 720});
    CHECK(r.w == 1280);
    CHECK(r.x == 0);
}
