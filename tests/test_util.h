#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

// A fresh, unique directory under the system temp dir.
inline std::filesystem::path temp_dir() {
    static std::atomic<int> counter{0};
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() /
               ("vp_tests_" + std::to_string(stamp) + "_" + std::to_string(counter++));
    std::filesystem::create_directories(dir);
    return dir;
}

inline void write_file(const std::filesystem::path& file, std::string_view content) {
    std::ofstream out(file, std::ios::binary);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

inline std::string u8_as_string(std::u8string_view s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}
