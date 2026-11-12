#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vp {

// Absolute, normalised, backslash-separated path with ASCII letters lower-cased.
std::string resume_key(const std::string& path_utf8);

// True when the position is worth coming back to: past the first 5 s and not within the last 10 s.
bool should_resume(double position, double duration);

class ResumeStore {
public:
    static constexpr std::size_t kMaxEntries = 500;

    explicit ResumeStore(std::filesystem::path file);
    std::optional<double> get(const std::string& key) const;
    void set(const std::string& key, double seconds);
    void clear(const std::string& key);
    bool save() const;

private:
    std::filesystem::path file_;
    std::vector<std::pair<std::string, double>> entries_;  // least recently set first
};

}  // namespace vp
