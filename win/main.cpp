#include <windows.h>

#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <vp/command_line.h>
#include <vp/settings.h>

#include "app_window.h"
#include "bench.h"
#include "paths.h"
#include "single_instance.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    std::vector<std::string> args;
    for (int i = 1; i < __argc; ++i) args.push_back(win::narrow(__wargv[i]));
    const vp::CommandLine cmd = vp::parse_command_line(args);

    // Made absolute here: a running instance may have a different working directory.
    std::optional<std::wstring> file;
    if (cmd.file_utf8) file = win::absolute_path(win::widen(*cmd.file_utf8));

    if (cmd.bench) win::bench_enable_trace();
    win::bench_trace("main entered");
    if (!cmd.bench && win::forward_to_running_instance(file)) return 0;

    vp::Settings settings;
    if (cmd.bench) settings.volume = 0;  // the benchmark measures, it should not beep
    else settings = vp::startup_settings(vp::load_settings(win::app_data_dir() / L"settings.ini"));

    // Order matters for start-up time. Opening the video (above all, starting the GPU) is the
    // slow part, so it is kicked off before the window is shown and the two overlap.
    win::AppWindow app;
    if (!app.create(instance, settings, cmd.bench)) return 1;
    win::bench_trace("window created");
    try {
        app.start_player();
    } catch (const std::exception&) {
        // Without mpv there is nothing to show; say so instead of vanishing.
        MessageBoxW(nullptr,
                    L"Zap could not start its video engine (libmpv).\n\n"
                    L"Reinstalling Zap or updating the graphics driver may help.",
                    L"Zap", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (file) app.open_file(win::narrow(*file));
    win::bench_trace("file requested");
    app.show();
    win::bench_trace("window shown");
    if (!file) PostMessageW(app.hwnd(), win::WM_APP_UI_READY, 0, 0);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
