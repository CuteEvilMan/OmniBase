#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fluxbase {

struct Metadata {
    std::uint32_t version{1};
    std::uint32_t block_size{8};
    std::uint32_t output_length{0};
    std::uint32_t charset_length{0};
    bool pow2{false};
    std::string charset;
};

struct Charset {
    std::string symbols;
    std::size_t radix{0};
    std::size_t effective_radix{0};
    std::size_t bits_per_symbol{0};
    bool pow2{false};
};

// Build charset with deduplication and pow2 trimming.
Charset build_charset(const std::string& raw, bool pow2);

// Compute L for a given block size and charset.
std::size_t compute_output_length(std::size_t block_size_bytes, std::size_t radix);

// Encode input file to output file using options and optional header.
void encode_file(const std::string& input_path,
                 const std::string& output_path,
                 const Charset& charset,
                 std::size_t block_size,
                 bool write_header);

// Decode input file to output file. If header is absent, caller must supply charset and block size.
void decode_file(const std::string& input_path,
                 const std::string& output_path,
                 const Charset* charset_override,
                 std::size_t block_size_override,
                 bool header_expected);

}  // namespace fluxbase
