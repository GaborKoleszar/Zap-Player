#include "paths.h"

#include <windows.h>

namespace win {

std::wstring widen(std::string_view utf8) {
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

std::string narrow(std::wstring_view utf16) {
    if (utf16.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::filesystem::path app_data_dir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    std::filesystem::path base = (n > 0 && n < MAX_PATH) ? std::filesystem::path(buf) : std::filesystem::temp_directory_path();
    return base / L"Zap";
}

std::filesystem::path cache_dir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    std::filesystem::path base = (n > 0 && n < MAX_PATH) ? std::filesystem::path(buf) : std::filesystem::temp_directory_path();
    return base / L"Zap" / L"cache";
}

std::wstring absolute_path(const std::wstring& path) {
    DWORD n = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (n == 0) return path;
    std::wstring out(n, L'\0');
    DWORD written = GetFullPathNameW(path.c_str(), n, out.data(), nullptr);
    if (written == 0 || written >= n) return path;
    out.resize(written);
    return out;
}

}  // namespace win
