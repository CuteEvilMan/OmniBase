#pragma once

#include <string>
#include <vector>

namespace fluxbase {

enum class Mode { Encode, Decode };

struct Options {
    Mode mode{Mode::Encode};
    std::string input_path;
    std::string output_path;
    std::string charset;
    bool pow2{false};
    bool no_header{false};
    std::size_t block_size{8};
    bool charset_provided{false};
};

// Parse CLI arguments; throws std::runtime_error on invalid usage.
Options parse_args(const std::vector<std::string>& args);

}  // namespace fluxbase
