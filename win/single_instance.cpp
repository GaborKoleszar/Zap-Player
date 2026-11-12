#include "single_instance.h"

#include <windows.h>

namespace win {

bool forward_to_running_instance(const std::optional<std::wstring>& absolute_file) {
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\Zap.SingleInstance");
    if (!mutex || GetLastError() != ERROR_ALREADY_EXISTS) return false;  // we are the first; keep the handle

    // The other instance may still be creating its window: give it a moment.
    HWND target = nullptr;
    for (int i = 0; i < 40 && !target; ++i) {
        target = FindWindowW(kMainWindowClass, nullptr);
        if (!target) Sleep(50);
    }
    // No window, or one that does not take the file (it is closing or hung): start normally.
    // The mutex handle is kept on purpose, so that later launches still find this instance.
    if (!target) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(target, &pid);
    AllowSetForegroundWindow(pid);

    std::wstring payload = absolute_file.value_or(L"");
    COPYDATASTRUCT data{};
    data.dwData = kCopyDataOpenFile;
    data.cbData = static_cast<DWORD>((payload.size() + 1) * sizeof(wchar_t));
    data.lpData = payload.data();
    DWORD_PTR accepted = 0;
    LRESULT sent = SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG,
                                       5000, &accepted);
    return sent != 0 && accepted != 0;
}

}  // namespace win
