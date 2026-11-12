#pragma once
#include <optional>
#include <string>

namespace win {

inline constexpr wchar_t kMainWindowClass[] = L"ZapMainWindow";
inline constexpr unsigned long long kCopyDataOpenFile = 1;

// If another instance is running, hands it the (absolute) file path, brings it to the front
// and returns true: the caller should exit. Otherwise claims the single-instance mutex and
// returns false.
bool forward_to_running_instance(const std::optional<std::wstring>& absolute_file);

}  // namespace win
