#include "fluxbase/cli.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace fluxbase
{

namespace
{

std::size_t parse_size(const std::string& value)
{
    try
    {
        std::size_t idx = 0;
        unsigned long parsed = std::stoul(value, &idx, 10);
        if (idx != value.size())
        {
            throw std::invalid_argument("trailing characters");
        }
        return static_cast<std::size_t>(parsed);
    }
    catch (const std::exception&)
    {
        throw std::runtime_error("Invalid numeric value: " + value);
    }
}

}  // namespace

Options parse_args(const std::vector<std::string>& args)
{
    if (args.empty())
    {
        throw std::runtime_error("Usage: fluxbase <encode|decode> [options]");
    }

    Options opts;
    const std::string& mode = args[0];
    if (mode == "encode")
    {
        opts.mode = Mode::Encode;
    }
    else if (mode == "decode")
    {
        opts.mode = Mode::Decode;
    }
    else
    {
        throw std::runtime_error("First argument must be 'encode' or 'decode'");
    }

    for (std::size_t i = 1; i < args.size(); ++i)
    {
        const std::string& tok = args[i];
        auto require_value = [&](const char* flag) -> const std::string&
        {
            if (i + 1 >= args.size())
            {
                throw std::runtime_error(std::string("Missing value for ") + flag);
            }
            return args[++i];
        };

        if (tok == "--input" || tok == "-i")
        {
            opts.input_path = require_value(tok.c_str());
        }
        else if (tok == "--output" || tok == "-o")
        {
            opts.output_path = require_value(tok.c_str());
        }
        else if (tok == "--charset" || tok == "-c")
        {
            opts.charset = require_value(tok.c_str());
            opts.charset_provided = true;
        }
        else if (tok == "--pow2")
        {
            opts.pow2 = true;
        }
        else if (tok == "--block" || tok == "-b")
        {
            opts.block_size = parse_size(require_value(tok.c_str()));
        }
        else if (tok == "--no-header")
        {
            opts.no_header = true;
        }
        else
        {
            throw std::runtime_error("Unknown option: " + tok);
        }
    }

    if (opts.input_path.empty() || opts.output_path.empty())
    {
        throw std::runtime_error("--input and --output are required");
    }

    if (opts.mode == Mode::Encode)
    {
        if (!opts.charset_provided)
        {
            throw std::runtime_error("--charset is required in encode mode");
        }
    }
    else
    {
        if (opts.no_header && !opts.charset_provided)
        {
            throw std::runtime_error("--charset is required for decode when --no-header is set");
        }
        if (opts.block_size == 0)
        {
            throw std::runtime_error("--block must be positive");
        }
    }

    return opts;
}

}  // namespace fluxbase
