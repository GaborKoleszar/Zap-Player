#include "vp/player.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <stdexcept>

#include <mpv/client.h>

namespace vp {
namespace {

enum Observed : uint64_t {
    kPause = 1, kTimePos, kDuration, kVolume, kMute, kSpeed, kEofReached, kTrackList,
};

const char* node_string(const mpv_node& map, const char* key) {
    if (map.format != MPV_FORMAT_NODE_MAP) return nullptr;
    for (int i = 0; i < map.u.list->num; ++i)
        if (std::strcmp(map.u.list->keys[i], key) == 0 && map.u.list->values[i].format == MPV_FORMAT_STRING)
            return map.u.list->values[i].u.string;
    return nullptr;
}

int64_t node_int(const mpv_node& map, const char* key) {
    for (int i = 0; i < map.u.list->num; ++i)
        if (std::strcmp(map.u.list->keys[i], key) == 0 && map.u.list->values[i].format == MPV_FORMAT_INT64)
            return map.u.list->values[i].u.int64;
    return 0;
}

bool node_flag(const mpv_node& map, const char* key) {
    for (int i = 0; i < map.u.list->num; ++i)
        if (std::strcmp(map.u.list->keys[i], key) == 0 && map.u.list->values[i].format == MPV_FORMAT_FLAG)
            return map.u.list->values[i].u.flag != 0;
    return false;
}

}  // namespace

struct Player::Impl {
    mpv_handle* mpv = nullptr;
    PlayerState state;
    std::vector<Track> tracks;
    bool awaiting_first_frame = false;
    uint64_t load_generation = 0;   // reply id of the most recent loadfile command
    int64_t current_entry_id = -1;  // mpv's playlist entry for that load, once known
    bool current_started = false;   // mpv has begun opening that entry: its time and length are its own
    std::mutex wakeup_mutex;
    std::function<void()> wakeup;

    void set(const char* name, const char* value) { mpv_set_option_string(mpv, name, value); }

    void command(std::initializer_list<const char*> args) {
        std::vector<const char*> argv(args);
        argv.push_back(nullptr);
        mpv_command_async(mpv, 0, argv.data());
    }

    void refresh_tracks() {
        tracks.clear();
        bool has_video = false;
        mpv_node root{};
        if (mpv_get_property(mpv, "track-list", MPV_FORMAT_NODE, &root) < 0) {
            state.has_video = false;
            return;
        }
        if (root.format == MPV_FORMAT_NODE_ARRAY) {
            for (int i = 0; i < root.u.list->num; ++i) {
                const mpv_node& item = root.u.list->values[i];
                if (item.format != MPV_FORMAT_NODE_MAP) continue;
                const char* type = node_string(item, "type");
                if (!type) continue;
                Track t;
                if (std::strcmp(type, "video") == 0) t.type = TrackType::Video;
                else if (std::strcmp(type, "audio") == 0) t.type = TrackType::Audio;
                else if (std::strcmp(type, "sub") == 0) t.type = TrackType::Subtitle;
                else continue;
                t.id = node_int(item, "id");
                if (const char* s = node_string(item, "title")) t.title = s;
                if (const char* s = node_string(item, "lang")) t.lang = s;
                t.selected = node_flag(item, "selected");
                t.external = node_flag(item, "external");
                if (t.type == TrackType::Video && t.selected) has_video = true;
                tracks.push_back(std::move(t));
            }
        }
        mpv_free_node_contents(&root);
        state.has_video = has_video;
    }
};

Player::Player(const PlayerOptions& options) : impl_(std::make_unique<Impl>()) {
    Impl& d = *impl_;
    d.mpv = mpv_create();
    if (!d.mpv) throw std::runtime_error("mpv_create failed");

    // Lean configuration: nothing that isn't needed to get the first frame on screen.
    d.set("config", "no");
    d.set("load-scripts", "no");
    d.set("osc", "no");
    d.set("osd-bar", "no");
    d.set("ytdl", "no");
    d.set("input-default-bindings", "no");
    d.set("input-vo-keyboard", "no");
    d.set("input-cursor", "no");
    d.set("cursor-autohide", "no");
    d.set("hwdec", "auto-safe");
    d.set("gpu-api", "d3d11");
    d.set("keep-open", "yes");
    d.set("idle", "yes");
    d.set("audio-file-auto", "no");
    // mpv's built-in scripts each start a script engine; none of them is used here.
    for (const char* script : {"load-stats-overlay", "load-console", "load-osd-console", "load-auto-profiles",
                               "load-select", "load-commands", "load-positioning", "load-context-menu"})
        d.set(script, "no");
    if (!options.log_file.empty()) d.set("log-file", options.log_file.c_str());
    if (!options.cache_dir.empty()) {
        d.set("gpu-shader-cache", "yes");
        d.set("gpu-shader-cache-dir", options.cache_dir.c_str());
    }

    if (options.wid != 0) {
        int64_t wid = options.wid;
        mpv_set_option(d.mpv, "wid", MPV_FORMAT_INT64, &wid);
    } else {
        d.set("vo", "null");
        d.set("ao", "null");
    }
    double volume = options.volume;
    mpv_set_option(d.mpv, "volume", MPV_FORMAT_DOUBLE, &volume);
    d.state.volume = options.volume;

    if (mpv_initialize(d.mpv) < 0) {
        mpv_terminate_destroy(d.mpv);
        throw std::runtime_error("mpv_initialize failed");
    }

    mpv_observe_property(d.mpv, kPause, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(d.mpv, kTimePos, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(d.mpv, kDuration, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(d.mpv, kVolume, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(d.mpv, kMute, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(d.mpv, kSpeed, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(d.mpv, kEofReached, "eof-reached", MPV_FORMAT_FLAG);
    mpv_observe_property(d.mpv, kTrackList, "track-list", MPV_FORMAT_NONE);

    mpv_set_wakeup_callback(d.mpv, [](void* ctx) {
        auto* impl = static_cast<Impl*>(ctx);
        std::lock_guard lock(impl->wakeup_mutex);
        if (impl->wakeup) impl->wakeup();
    }, &d);
}

Player::~Player() {
    if (impl_->mpv) {
        mpv_set_wakeup_callback(impl_->mpv, nullptr, nullptr);
        mpv_terminate_destroy(impl_->mpv);
    }
}

void Player::load(const std::string& path_utf8, double start_seconds) {
    Impl& d = *impl_;
    d.awaiting_first_frame = true;
    d.state.at_end = false;
    // Nothing of the previous file is reported from here on. Until mpv starts on the new entry,
    // time and length updates still belong to the old file and are dropped in poll().
    d.state.has_file = false;
    d.state.position = d.state.duration = 0;
    d.current_started = false;
    set_pause(false);  // mpv keeps the pause state across files; a newly opened file should play
    // The command's reply names the playlist entry, which is how a failure is later matched
    // to the file that is current rather than to one that was already replaced.
    d.current_entry_id = -1;
    ++d.load_generation;
    char opt[64];
    std::snprintf(opt, sizeof(opt), "start=%.3f", start_seconds);
    const char* with_start[] = {"loadfile", path_utf8.c_str(), "replace", "-1", opt, nullptr};
    const char* plain[] = {"loadfile", path_utf8.c_str(), "replace", nullptr};
    mpv_command_async(d.mpv, d.load_generation, start_seconds > 0 ? with_start : plain);
}

void Player::set_pause(bool paused) {
    int flag = paused ? 1 : 0;
    mpv_set_property_async(impl_->mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void Player::toggle_pause() {
    // At the end of a file "play" means "play again from the start".
    if (impl_->state.at_end) {
        seek_absolute(0);
        set_pause(false);
        return;
    }
    set_pause(!impl_->state.paused);
}

void Player::seek_relative(double seconds) {
    char amount[32];
    std::snprintf(amount, sizeof(amount), "%.3f", seconds);
    impl_->command({"seek", amount, "relative+exact"});
}

void Player::seek_absolute(double seconds, SeekMode mode) {
    char amount[32];
    std::snprintf(amount, sizeof(amount), "%.3f", seconds < 0 ? 0.0 : seconds);
    impl_->command({"seek", amount, mode == SeekMode::Fast ? "absolute+keyframes" : "absolute+exact"});
}

void Player::set_volume(int percent) {
    double v = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    mpv_set_property_async(impl_->mpv, 0, "volume", MPV_FORMAT_DOUBLE, &v);
}

void Player::toggle_mute() { impl_->command({"cycle", "mute"}); }

void Player::set_speed(double speed) {
    mpv_set_property_async(impl_->mpv, 0, "speed", MPV_FORMAT_DOUBLE, &speed);
}

const std::vector<Track>& Player::tracks() const { return impl_->tracks; }

void Player::select_audio(int64_t id) {
    mpv_set_property_async(impl_->mpv, 0, "aid", MPV_FORMAT_INT64, &id);
}

void Player::select_subtitle(int64_t id) {
    if (id == 0) {
        const char* no = "no";
        mpv_set_property_async(impl_->mpv, 0, "sid", MPV_FORMAT_STRING, &no);
    } else {
        mpv_set_property_async(impl_->mpv, 0, "sid", MPV_FORMAT_INT64, &id);
    }
}

void Player::set_subtitle_lift(double fraction) {
    double clamped = fraction < 0 ? 0 : fraction > 0.5 ? 0.5 : fraction;
    double pos = 100.0 - clamped * 100.0;
    mpv_set_property_async(impl_->mpv, 0, "sub-pos", MPV_FORMAT_DOUBLE, &pos);
}

double Player::subtitle_position() const {
    double pos = 100.0;
    mpv_get_property(impl_->mpv, "sub-pos", MPV_FORMAT_DOUBLE, &pos);
    return pos;
}

const PlayerState& Player::state() const { return impl_->state; }

std::string Player::hwdec_current() const {
    char* value = mpv_get_property_string(impl_->mpv, "hwdec-current");
    std::string out = value ? value : "no";
    mpv_free(value);
    return out;
}

void Player::set_wakeup(std::function<void()> wakeup) {
    std::lock_guard lock(impl_->wakeup_mutex);
    impl_->wakeup = std::move(wakeup);
}

std::vector<PlayerEvent> Player::poll() {
    Impl& d = *impl_;
    std::vector<PlayerEvent> out;
    bool state_changed = false, tracks_changed = false;

    for (;;) {
        mpv_event* ev = mpv_wait_event(d.mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE) break;
        switch (ev->event_id) {
            case MPV_EVENT_START_FILE: {
                auto* start = static_cast<mpv_event_start_file*>(ev->data);
                if (start && start->playlist_entry_id == d.current_entry_id) d.current_started = true;
                break;
            }
            case MPV_EVENT_FILE_LOADED:
                if (!d.current_started) break;  // a file that a newer load has already replaced
                d.state.has_file = true;
                d.state.at_end = false;
                tracks_changed = true;
                out.push_back(PlayerEvent::FileLoaded);
                break;
            case MPV_EVENT_PLAYBACK_RESTART:
                if (d.awaiting_first_frame && d.state.has_file) {
                    d.awaiting_first_frame = false;
                    out.push_back(PlayerEvent::FirstFrame);
                }
                break;
            case MPV_EVENT_COMMAND_REPLY: {
                if (ev->reply_userdata != d.load_generation) break;  // an older load, or another command
                if (ev->error < 0) {
                    d.state.has_file = false;
                    d.awaiting_first_frame = false;
                    out.push_back(PlayerEvent::LoadFailed);
                    break;
                }
                auto* reply = static_cast<mpv_event_command*>(ev->data);
                if (reply && reply->result.format == MPV_FORMAT_NODE_MAP)
                    d.current_entry_id = node_int(reply->result, "playlist_entry_id");
                break;
            }
            case MPV_EVENT_END_FILE: {
                auto* end = static_cast<mpv_event_end_file*>(ev->data);
                // A file that was already replaced by a newer load: nothing about it matters now.
                if (end->playlist_entry_id != d.current_entry_id) break;
                // Whatever the reason, the old file's position must not leak into the next one.
                d.state.position = d.state.duration = 0;
                if (end->reason == MPV_END_FILE_REASON_ERROR) {
                    d.state.has_file = false;
                    d.awaiting_first_frame = false;
                    tracks_changed = true;
                    out.push_back(PlayerEvent::LoadFailed);
                } else if (end->reason != MPV_END_FILE_REASON_REDIRECT) {
                    // Replaced by another loadfile, or stopped: the next FILE_LOADED sets it again.
                    d.state.has_file = false;
                    state_changed = true;
                }
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE: {
                auto* prop = static_cast<mpv_event_property*>(ev->data);
                if (ev->reply_userdata == kTrackList) {
                    tracks_changed = true;
                    break;
                }
                if (!prop->data) break;
                state_changed = true;
                switch (ev->reply_userdata) {
                    case kPause: d.state.paused = *static_cast<int*>(prop->data) != 0; break;
                    case kTimePos:
                        if (d.current_started) d.state.position = *static_cast<double*>(prop->data);
                        break;
                    case kDuration:
                        if (d.current_started) d.state.duration = *static_cast<double*>(prop->data);
                        break;
                    case kVolume: d.state.volume = static_cast<int>(std::lround(*static_cast<double*>(prop->data))); break;
                    case kMute: d.state.muted = *static_cast<int*>(prop->data) != 0; break;
                    case kSpeed: d.state.speed = *static_cast<double*>(prop->data); break;
                    case kEofReached: {
                        bool eof = *static_cast<int*>(prop->data) != 0;
                        if (eof && !d.state.at_end) out.push_back(PlayerEvent::EndOfFile);
                        d.state.at_end = eof;
                        break;
                    }
                    default: break;
                }
                break;
            }
            default: break;
        }
    }

    if (tracks_changed) {
        d.refresh_tracks();
        out.push_back(PlayerEvent::TracksChanged);
    }
    if (state_changed) out.push_back(PlayerEvent::StateChanged);
    return out;
}

}  // namespace vp
