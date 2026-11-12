#pragma once
#include <optional>
#include <string>
#include <vector>

namespace vp {

struct CommandLine {
    std::optional<std::string> file_utf8;
    bool bench = false;
};

// args exclude the program name. "--x" arguments are switches; the first other argument is the file.
CommandLine parse_command_line(const std::vector<std::string>& args_utf8);

}  // namespace vp
