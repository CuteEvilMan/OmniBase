#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "fluxbase/cli.hpp"
#include "fluxbase/codec.hpp"

int main(int argc, char** argv)
{
    try
    {
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i)
        {
            args.emplace_back(argv[i]);
        }
        fluxbase::Options options = fluxbase::parse_args(args);
        fluxbase::Charset charset = fluxbase::build_charset(options.charset, options.pow2);

        if (options.mode == fluxbase::Mode::Encode)
        {
            fluxbase::encode_file(options.input_path, options.output_path, charset,
                                  options.block_size, !options.no_header);
        }
        else
        {
            const fluxbase::Charset* charset_ptr = options.charset_provided ? &charset : nullptr;
            fluxbase::decode_file(options.input_path, options.output_path, charset_ptr,
                                  options.block_size, !options.no_header);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
