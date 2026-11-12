#include "vp/settings.h"

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace vp {
namespace {

std::optional<int> to_int(std::string_view s) {
    int v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || p != s.data() + s.size()) return std::nullopt;
    return v;
}

std::optional<Rect> to_rect(std::string_view s) {
    int v[4];
    for (int i = 0; i < 4; ++i) {
        auto comma = s.find(',');
        if ((i < 3) == (comma == std::string_view::npos)) return std::nullopt;
        auto n = to_int(s.substr(0, comma));
        if (!n) return std::nullopt;
        v[i] = *n;
        if (i < 3) s.remove_prefix(comma + 1);
    }
    if (v[2] <= 0 || v[3] <= 0) return std::nullopt;
    return Rect{v[0], v[1], v[2], v[3]};
}

}  // namespace

Settings load_settings(const std::filesystem::path& file) {
    Settings s;
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string_view key(line.data(), eq), value(line.data() + eq + 1, line.size() - eq - 1);
        if (key == "startup") {
            if (value == "remember") s.startup = StartupMode::RememberLast;
            else if (value == "defaults") s.startup = StartupMode::Defaults;
        } else if (key == "layout") {
            if (value == "floating") s.layout = ControlLayout::Floating;
            else if (value == "docked") s.layout = ControlLayout::Docked;
        } else if (key == "volume") {
            if (auto v = to_int(value); v && *v >= 0 && *v <= 100) s.volume = *v;
        } else if (key == "borderless") {
            if (value == "1") s.borderless = true;
            else if (value == "0") s.borderless = false;
        } else if (key == "maximized") {
            if (value == "1") s.maximized = true;
            else if (value == "0") s.maximized = false;
        } else if (key == "window") {
            if (auto r = to_rect(value)) s.window = r;
        }
    }
    return s;
}

Settings startup_settings(const Settings& saved) {
    if (saved.startup == StartupMode::RememberLast) return saved;
    Settings fresh;
    fresh.startup = StartupMode::Defaults;
    return fresh;
}

bool save_settings(const std::filesystem::path& file, const Settings& s) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    auto tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << "startup=" << (s.startup == StartupMode::Defaults ? "defaults" : "remember") << '\n';
        out << "layout=" << (s.layout == ControlLayout::Docked ? "docked" : "floating") << '\n';
        out << "volume=" << s.volume << '\n';
        out << "borderless=" << (s.borderless ? 1 : 0) << '\n';
        out << "maximized=" << (s.maximized ? 1 : 0) << '\n';
        if (s.window)
            out << "window=" << s.window->x << ',' << s.window->y << ',' << s.window->w << ',' << s.window->h << '\n';
        if (!out.flush()) return false;
    }
    std::filesystem::rename(tmp, file, ec);
    return !ec;
}

}  // namespace vp
