#pragma once

namespace win {

// Appends the milliseconds from process creation to now to the file zap-bench.txt in
// the temp folder. Called once, when the first video frame has been presented in --bench mode.
void bench_report_first_frame();

// Milliseconds since this process was created.
long long ms_since_process_start();

// In --bench mode, bench_trace appends "<ms> <label>" to zap-trace.txt in the temp
// folder. Otherwise it does nothing.
void bench_enable_trace();
void bench_trace(const char* label);

}  // namespace win
