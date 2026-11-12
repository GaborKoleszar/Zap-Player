#pragma once
#include <filesystem>
#include <optional>

#include "vp/geometry.h"

namespace vp {

enum class ControlLayout { Floating, Docked };

// What the next start begins with: the state the last session ended in, or the built-in defaults.
enum class StartupMode { RememberLast, Defaults };

struct Settings {
    StartupMode startup = StartupMode::RememberLast;
    ControlLayout layout = ControlLayout::Floating;
    int volume = 100;
    bool borderless = false;
    bool maximized = false;
    std::optional<Rect> window;
};

// A missing or damaged file gives defaults; bad lines are skipped one by one.
Settings load_settings(const std::filesystem::path& file);
// The settings a new session starts with, given what was saved: the saved ones, or, when the
// saved startup mode is Defaults, the defaults (keeping only that choice).
Settings startup_settings(const Settings& saved);
// Writes through a temporary file; creates the parent directory.
bool save_settings(const std::filesystem::path& file, const Settings& s);

}  // namespace vp
