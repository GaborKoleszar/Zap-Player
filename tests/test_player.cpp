#include <doctest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

#include <vp/player.h>

using namespace std::chrono_literals;

namespace {

std::string media(const char* name) { return std::string(VP_MEDIA_DIR) + "/" + name; }

bool media_available() { return std::filesystem::exists(media("sample.mkv")); }

// Pumps poll() until `done` returns true or the timeout passes.
bool pump_until(vp::Player& p, const std::function<bool(const std::vector<vp::PlayerEvent>&)>& done,
                std::chrono::milliseconds timeout = 5s) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto events = p.poll();
        if (done(events)) return true;
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

bool wait_for(vp::Player& p, vp::PlayerEvent wanted, std::chrono::milliseconds timeout = 5s) {
    return pump_until(p, [&](const auto& events) {
        return std::find(events.begin(), events.end(), wanted) != events.end();
    }, timeout);
}

bool wait_state(vp::Player& p, const std::function<bool()>& pred) {
    return pump_until(p, [&](const auto&) { return pred(); }, 3s);
}

int count(const std::vector<vp::Track>& t, vp::TrackType type) {
    return static_cast<int>(std::count_if(t.begin(), t.end(), [&](const vp::Track& x) { return x.type == type; }));
}

int64_t nth_id(const std::vector<vp::Track>& t, vp::TrackType type, int n) {
    for (const auto& x : t)
        if (x.type == type && n-- == 0) return x.id;
    return -1;
}

int64_t selected_id(const std::vector<vp::Track>& t, vp::TrackType type) {
    for (const auto& x : t)
        if (x.type == type && x.selected) return x.id;
    return 0;
}

}  // namespace

#define REQUIRE_MEDIA()                                                                        \
    if (!media_available()) {                                                                  \
        MESSAGE("sample media missing; run bench/make-sample.ps1 (test skipped)");            \
        return;                                                                                \
    }

TEST_CASE("player loads a file and reports state and tracks") {
    REQUIRE_MEDIA();
    vp::Player p({});
    CHECK_FALSE(p.state().has_file);
    p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    REQUIRE(wait_state(p, [&] { return p.state().duration > 0 && !p.tracks().empty(); }));
    CHECK(p.state().has_file);
    CHECK(p.state().has_video);
    CHECK(p.state().duration == doctest::Approx(10).epsilon(0.1));
    const auto& t = p.tracks();
    CHECK(count(t, vp::TrackType::Audio) == 2);
    CHECK(count(t, vp::TrackType::Subtitle) == 2);  // embedded + sample.srt next to the file
    CHECK(std::any_of(t.begin(), t.end(), [](const vp::Track& x) { return x.external; }));
    CHECK(std::any_of(t.begin(), t.end(), [](const vp::Track& x) { return x.lang == "hun"; }));
}

TEST_CASE("player pause, seek, speed, volume and mute") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    p.set_pause(true);
    p.seek_absolute(5);
    p.set_speed(1.5);
    p.set_volume(40);
    p.toggle_mute();
    CHECK(wait_state(p, [&] { return p.state().paused; }));
    CHECK(wait_state(p, [&] { return p.state().position > 4.5 && p.state().position < 5.5; }));
    CHECK(wait_state(p, [&] { return p.state().speed == doctest::Approx(1.5); }));
    CHECK(wait_state(p, [&] { return p.state().volume == 40; }));
    CHECK(wait_state(p, [&] { return p.state().muted; }));
    p.seek_relative(-3);
    CHECK(wait_state(p, [&] { return p.state().position > 1.5 && p.state().position < 2.5; }));
    p.toggle_pause();
    CHECK(wait_state(p, [&] { return !p.state().paused; }));
}

TEST_CASE("player switches audio and subtitle tracks") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    REQUIRE(wait_state(p, [&] { return count(p.tracks(), vp::TrackType::Audio) == 2; }));
    auto second_audio = nth_id(p.tracks(), vp::TrackType::Audio, 1);
    auto second_sub = nth_id(p.tracks(), vp::TrackType::Subtitle, 1);
    p.select_audio(second_audio);
    p.select_subtitle(second_sub);
    CHECK(wait_state(p, [&] { return selected_id(p.tracks(), vp::TrackType::Audio) == second_audio; }));
    CHECK(wait_state(p, [&] { return selected_id(p.tracks(), vp::TrackType::Subtitle) == second_sub; }));
    p.select_subtitle(0);
    CHECK(wait_state(p, [&] { return selected_id(p.tracks(), vp::TrackType::Subtitle) == 0; }));
}

TEST_CASE("player honours the start position and keeps the file open at the end") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(media("sample.mkv"), 8.0);
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    REQUIRE(wait_state(p, [&] { return p.state().position > 0; }));
    CHECK(p.state().position >= 7.5);
    REQUIRE(wait_for(p, vp::PlayerEvent::EndOfFile));
    CHECK(p.state().has_file);
    CHECK(p.state().at_end);
}

TEST_CASE("player loads audio-only and no-audio files") {
    REQUIRE_MEDIA();
    {
        vp::Player p({});
        p.load(media("audio-only.m4a"));
        REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
        CHECK_FALSE(p.state().has_video);
    }
    {
        vp::Player p({});
        p.load(media("no-audio.mp4"));
        REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
        REQUIRE(wait_state(p, [&] { return !p.tracks().empty(); }));
        CHECK(p.state().has_video);
        CHECK(count(p.tracks(), vp::TrackType::Audio) == 0);
    }
}

TEST_CASE("player reports LoadFailed for broken and missing files, then can load a good one") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(media("broken.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::LoadFailed));
    CHECK_FALSE(p.state().has_file);
    p.load(media("does-not-exist.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::LoadFailed));
    p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    CHECK(p.state().has_file);
}

TEST_CASE("player opens a file with a non-ASCII name") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(std::string(VP_MEDIA_DIR) + "/" +
           std::string(reinterpret_cast<const char*>(u8"Árvíztűrő tükörfúrógép.mkv")));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
}

TEST_CASE("player wakeup callback fires when events are pending") {
    REQUIRE_MEDIA();
    vp::Player p({});
    std::atomic<int> wakeups{0};
    p.set_wakeup([&] { ++wakeups; });
    p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    CHECK(wakeups.load() > 0);
}

TEST_CASE("player starts playing a newly opened file even if the previous one was paused") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    p.set_pause(true);
    REQUIRE(wait_state(p, [&] { return p.state().paused; }));
    p.load(media("no-audio.mp4"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    CHECK(wait_state(p, [&] { return !p.state().paused; }));
}

TEST_CASE("player lifts subtitles by a fraction of the picture height") {
    REQUIRE_MEDIA();
    vp::Player p({});
    CHECK(p.subtitle_position() == doctest::Approx(100));
    p.set_subtitle_lift(0.12);
    CHECK(wait_state(p, [&] { return p.subtitle_position() == doctest::Approx(88); }));
    p.set_subtitle_lift(0);
    CHECK(wait_state(p, [&] { return p.subtitle_position() == doctest::Approx(100); }));
    p.set_subtitle_lift(5);  // nonsense input is clamped to half the height
    CHECK(wait_state(p, [&] { return p.subtitle_position() == doctest::Approx(50); }));
}

TEST_CASE("player does not report the previous file's position for a newly loaded file") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(media("sample.mkv"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    p.set_pause(true);
    p.seek_absolute(8);
    REQUIRE(wait_state(p, [&] { return p.state().position > 7.5; }));
    p.load(media("no-audio.mp4"));
    // From the moment of the load nothing of the old file is reported any more: a caller that
    // saves "the current file's position" must not get the previous file's.
    CHECK_FALSE(p.state().has_file);
    CHECK(p.state().position == 0);
    CHECK(p.state().duration == 0);
    bool stale = false;
    REQUIRE(pump_until(p, [&](const std::vector<vp::PlayerEvent>& events) {
        bool loaded = std::find(events.begin(), events.end(), vp::PlayerEvent::FileLoaded) != events.end();
        if (!p.state().has_file && (p.state().position != 0 || p.state().duration != 0)) stale = true;
        if (loaded && p.state().position > 5) stale = true;
        return loaded;
    }));
    CHECK_FALSE(stale);
}

TEST_CASE("a late failure of an earlier file does not disturb the file opened after it") {
    REQUIRE_MEDIA();
    for (int round = 0; round < 5; ++round) {
        vp::Player p({});
        p.load(media("broken.mkv"));
        if (round > 0) {
            // Let the broken file get part-way before the next one replaces it.
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(round * 2);
            while (std::chrono::steady_clock::now() < until) std::this_thread::yield();
        }
        p.load(media("sample.mkv"));
        bool failed = false, first_frame = false;
        pump_until(p, [&](const std::vector<vp::PlayerEvent>& events) {
            for (auto e : events) {
                if (e == vp::PlayerEvent::LoadFailed) failed = true;
                if (e == vp::PlayerEvent::FirstFrame) first_frame = true;
            }
            return first_frame;
        });
        CHECK(first_frame);
        CHECK_FALSE(failed);
        CHECK(p.state().has_file);
    }
}

TEST_CASE("player fast seek lands near the target and exact seek lands on it") {
    REQUIRE_MEDIA();
    vp::Player p({});
    p.load(media("long.mp4"));
    REQUIRE(wait_for(p, vp::PlayerEvent::FirstFrame));
    p.set_pause(true);
    p.seek_absolute(30, vp::SeekMode::Fast);
    CHECK(wait_state(p, [&] { return p.state().position > 15 && p.state().position < 45; }));
    p.seek_absolute(30, vp::SeekMode::Exact);
    CHECK(wait_state(p, [&] { return p.state().position > 29.9 && p.state().position < 30.1; }));
}
