#include "vp/resume_store.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <system_error>

namespace vp {

std::string resume_key(const std::string& path_utf8) {
    std::u8string u8(reinterpret_cast<const char8_t*>(path_utf8.data()), path_utf8.size());
    std::error_code ec;
    std::filesystem::path p(u8);
    auto abs = std::filesystem::absolute(p, ec);
    if (ec) abs = p;
    auto norm = abs.lexically_normal().make_preferred().u8string();
    std::string key(reinterpret_cast<const char*>(norm.data()), norm.size());
    for (char& c : key) {
        if (c == '/') c = '\\';
        else if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return key;
}

bool should_resume(double position, double duration) {
    return std::isfinite(position) && std::isfinite(duration) && position >= 5 && duration - position >= 10;
}

ResumeStore::ResumeStore(std::filesystem::path file) : file_(std::move(file)) {
    std::ifstream in(file_, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tab = line.find('\t');
        if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size()) continue;
        double seconds = 0;
        auto [p, ec] = std::from_chars(line.data(), line.data() + tab, seconds);
        if (ec != std::errc{} || p != line.data() + tab || !std::isfinite(seconds) || seconds < 0) continue;
        set(line.substr(tab + 1), seconds);
    }
}

std::optional<double> ResumeStore::get(const std::string& key) const {
    for (const auto& e : entries_)
        if (e.first == key) return e.second;
    return std::nullopt;
}

void ResumeStore::set(const std::string& key, double seconds) {
    clear(key);
    entries_.emplace_back(key, seconds);
    if (entries_.size() > kMaxEntries) entries_.erase(entries_.begin());
}

void ResumeStore::clear(const std::string& key) {
    std::erase_if(entries_, [&](const auto& e) { return e.first == key; });
}

bool ResumeStore::save() const {
    std::error_code ec;
    std::filesystem::create_directories(file_.parent_path(), ec);
    auto tmp = file_;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        for (const auto& e : entries_) out << e.second << '\t' << e.first << '\n';
        if (!out.flush()) return false;
    }
    std::filesystem::rename(tmp, file_, ec);
    return !ec;
}

}  // namespace vp
