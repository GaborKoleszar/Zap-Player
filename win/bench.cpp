#include "bench.h"

#include <windows.h>
#include <dwmapi.h>

#include <cstdio>
#include <string>

namespace win {

namespace {

bool g_trace = false;

std::wstring temp_file(const wchar_t* name) {
    wchar_t temp[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, temp)) return {};
    return std::wstring(temp) + name;
}

}  // namespace

long long ms_since_process_start() {
    FILETIME created, exited, kernel, user, now;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    GetSystemTimePreciseAsFileTime(&now);
    ULARGE_INTEGER a, b;
    a.LowPart = created.dwLowDateTime;
    a.HighPart = created.dwHighDateTime;
    b.LowPart = now.dwLowDateTime;
    b.HighPart = now.dwHighDateTime;
    return static_cast<long long>((b.QuadPart - a.QuadPart) / 10000);
}

void bench_enable_trace() { g_trace = true; }

void bench_trace(const char* label) {
    if (!g_trace) return;
    long long ms = ms_since_process_start();
    if (FILE* f = _wfopen(temp_file(L"zap-trace.txt").c_str(), L"a")) {
        std::fprintf(f, "%lld %s\n", ms, label);
        std::fclose(f);
    }
}

void bench_report_first_frame() {
    DwmFlush();  // wait for the compositor to pick the frame up
    long long ms = ms_since_process_start();
    bench_trace("first frame presented");
    if (FILE* f = _wfopen(temp_file(L"zap-bench.txt").c_str(), L"a")) {
        std::fprintf(f, "%lld\n", ms);
        std::fclose(f);
    }
}

}  // namespace win
