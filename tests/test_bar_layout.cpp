#include <doctest.h>
#include <vp/bar_layout.h>

TEST_CASE("docked layout is one row, in order, inside bounds") {
    auto l = vp::compute_bar_layout(vp::BarStyle::Docked, 900, 48);
    CHECK(l.play.x < l.time.x);
    CHECK(l.time.x < l.seek.x);
    CHECK(l.seek.x < l.volume.x);
    CHECK(l.volume.x < l.subtitles.x);
    CHECK(l.subtitles.x < l.audio.x);
    CHECK(l.audio.x < l.speed.x);
    CHECK(l.speed.x < l.borderless.x);
    CHECK(l.borderless.x < l.fullscreen.x);
    CHECK(l.play.x == 12);
    CHECK(l.fullscreen.x + l.fullscreen.w == doctest::Approx(900 - 12));
    CHECK(l.play.w == 32);
    CHECK(l.play.h == 32);
    CHECK(l.play.y == 8);
    CHECK(l.volume.w == 80);
    CHECK(l.time.x + l.time.w <= l.seek.x);
    CHECK(l.seek.x + l.seek.w <= l.volume.x);
}
TEST_CASE("floating layout puts seek on its own full-width row above the buttons") {
    auto l = vp::compute_bar_layout(vp::BarStyle::Floating, 720, 72);
    CHECK(l.seek.y + l.seek.h <= l.play.y);
    CHECK(l.seek.w == doctest::Approx(720 - 24));
    CHECK(l.play.y + l.play.h <= 72);
    CHECK(l.fullscreen.x + l.fullscreen.w == doctest::Approx(720 - 12));
    CHECK(l.volume.x + l.volume.w <= l.subtitles.x);
}
TEST_CASE("narrow docked widths drop volume, then time, and keep seek >= 80") {
    auto a = vp::compute_bar_layout(vp::BarStyle::Docked, 500, 48);
    CHECK(a.volume.w == 0);
    CHECK(a.time.w > 0);
    CHECK(a.seek.w >= 80);
    auto b = vp::compute_bar_layout(vp::BarStyle::Docked, 400, 48);
    CHECK(b.volume.w == 0);
    CHECK(b.time.w == 0);
    CHECK(b.seek.w >= 80);
}
TEST_CASE("narrow floating widths drop volume, then time, without overlap") {
    auto a = vp::compute_bar_layout(vp::BarStyle::Floating, 400, 72);
    CHECK(a.volume.w == 0);
    CHECK(a.time.w > 0);
    CHECK(a.time.x + a.time.w <= a.subtitles.x);
    auto b = vp::compute_bar_layout(vp::BarStyle::Floating, 290, 72);
    CHECK(b.time.w == 0);
    CHECK(b.play.x + b.play.w <= b.subtitles.x);
}
TEST_CASE("hit testing and seek fraction") {
    auto l = vp::compute_bar_layout(vp::BarStyle::Docked, 900, 48);
    CHECK(vp::hit_test(l, l.play.x + 5, l.play.y + 5) == vp::BarElement::PlayPause);
    CHECK(vp::hit_test(l, l.seek.x + l.seek.w / 2, l.seek.y + l.seek.h / 2) == vp::BarElement::Seek);
    CHECK(vp::hit_test(l, l.fullscreen.x + 1, l.fullscreen.y + 1) == vp::BarElement::Fullscreen);
    CHECK(vp::hit_test(l, 1, 1) == vp::BarElement::None);
    CHECK(vp::seek_fraction(l, l.seek.x - 50) == 0.0);
    CHECK(vp::seek_fraction(l, l.seek.x + l.seek.w / 2) == doctest::Approx(0.5));
    CHECK(vp::seek_fraction(l, 5000) == 1.0);
    // The ends of the drawn track, not of the wider seek area, are 0 and 1.
    CHECK(vp::seek_fraction(l, l.seek.x + vp::kSeekTrackInset) == 0.0);
    CHECK(vp::seek_fraction(l, l.seek.x + l.seek.w - vp::kSeekTrackInset) == 1.0);
    CHECK(vp::seek_fraction(l, l.seek.x + vp::kSeekTrackInset + 1) > 0.0);
}
TEST_CASE("a dropped element cannot be hit") {
    auto l = vp::compute_bar_layout(vp::BarStyle::Docked, 400, 48);
    CHECK(vp::hit_test(l, l.volume.x, l.volume.y) != vp::BarElement::Volume);
}
