#include "vp/command_line.h"

namespace vp {

CommandLine parse_command_line(const std::vector<std::string>& args_utf8) {
    CommandLine out;
    for (const auto& arg : args_utf8) {
        if (arg.starts_with("--")) {
            if (arg == "--bench") out.bench = true;
        } else if (!arg.empty() && !out.file_utf8) {
            out.file_utf8 = arg;
        }
    }
    return out;
}

}  // namespace vp
