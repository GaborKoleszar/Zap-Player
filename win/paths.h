#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace win {

std::wstring widen(std::string_view utf8);
std::string narrow(std::wstring_view utf16);

// %APPDATA%\Zap
std::filesystem::path app_data_dir();

// %LOCALAPPDATA%/Zap/cache: compiled shaders, safe to delete.
std::filesystem::path cache_dir();

// Absolute form of a path given on the command line or dropped on the window.
std::wstring absolute_path(const std::wstring& path);

}  // namespace win
