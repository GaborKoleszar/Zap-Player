#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace vp {

enum class TrackType { Video, Audio, Subtitle };

struct Track {
    int64_t id = 0;
    TrackType type = TrackType::Video;
    std::string title;
    std::string lang;
    bool selected = false;
    bool external = false;
};

struct PlayerState {
    bool has_file = false;
    bool paused = false;
    double position = 0;
    double duration = 0;
    int volume = 100;
    bool muted = false;
    double speed = 1.0;
    bool has_video = false;
    bool at_end = false;
};

// Fast seeks jump to the nearest keyframe (for dragging); Exact seeks land on the requested time.
enum class SeekMode { Fast, Exact };

enum class PlayerEvent { FileLoaded, FirstFrame, EndOfFile, LoadFailed, StateChanged, TracksChanged };

struct PlayerOptions {
    int64_t wid = 0;   // native window handle to render into; 0 = no video and no audio output (tests)
    int volume = 100;
    std::string log_file;   // UTF-8 path for mpv's verbose log; empty = no log (diagnostics only)
    std::string cache_dir;  // UTF-8 folder for compiled-shader caches; empty = no caching
};

// Thin wrapper around libmpv. All methods must be called on the thread that owns the player.
class Player {
public:
    explicit Player(const PlayerOptions& options);
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    void load(const std::string& path_utf8, double start_seconds = 0);
    void set_pause(bool paused);
    void toggle_pause();
    void seek_relative(double seconds);
    void seek_absolute(double seconds, SeekMode mode = SeekMode::Exact);
    void set_volume(int percent);
    void toggle_mute();
    void set_speed(double speed);
    const std::vector<Track>& tracks() const;
    void select_audio(int64_t id);
    void select_subtitle(int64_t id);  // 0 turns subtitles off
    // Moves subtitles up by this fraction of the picture height (0 = normal position), so that
    // overlay controls at the bottom do not cover them.
    void set_subtitle_lift(double fraction);
    double subtitle_position() const;  // mpv's sub-pos: 100 = normal, smaller = higher
    const PlayerState& state() const;  // updated by poll()
    std::string hwdec_current() const;  // e.g. "d3d11va", or "no"

    // Called from an mpv thread whenever poll() has work to do.
    void set_wakeup(std::function<void()> wakeup);
    // Drains pending mpv events, updates state() and tracks(), and reports what happened.
    std::vector<PlayerEvent> poll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace vp
