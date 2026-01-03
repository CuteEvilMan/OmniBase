#include "fluxbase/codec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fluxbase {

namespace {

constexpr char kMagic[4] = {'F', 'L', 'X', 'B'};
constexpr std::uint32_t kVersion = 1;

struct HeaderBinary {
    char magic[4];
    std::uint32_t version;
    std::uint8_t pow2;
    std::uint32_t block_size;
    std::uint32_t output_length;
    std::uint32_t charset_length;
};

void write_header(std::ofstream& out, const Metadata& meta) {
    HeaderBinary h{};
    std::copy(std::begin(kMagic), std::end(kMagic), h.magic);
    h.version = kVersion;
    h.pow2 = meta.pow2 ? 1U : 0U;
    h.block_size = meta.block_size;
    h.output_length = meta.output_length;
    h.charset_length = static_cast<std::uint32_t>(meta.charset.size());
    out.write(reinterpret_cast<const char*>(&h), sizeof(h));
    out.write(meta.charset.data(), static_cast<std::streamsize>(meta.charset.size()));
}

Metadata read_header(std::ifstream& in) {
    HeaderBinary h{};
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(h))) {
        throw std::runtime_error("Failed to read header");
    }
    if (!std::equal(std::begin(kMagic), std::end(kMagic), h.magic)) {
        throw std::runtime_error("Invalid header magic");
    }
    if (h.version != kVersion) {
        throw std::runtime_error("Unsupported version");
    }
    Metadata meta;
    meta.version = h.version;
    meta.block_size = h.block_size;
    meta.output_length = h.output_length;
    meta.pow2 = h.pow2 != 0;
    meta.charset_length = h.charset_length;
    meta.charset.resize(h.charset_length);
    in.read(meta.charset.data(), static_cast<std::streamsize>(meta.charset_length));
    if (in.gcount() != static_cast<std::streamsize>(meta.charset_length)) {
        throw std::runtime_error("Incomplete charset in header");
    }
    return meta;
}

std::size_t index_in_charset(char c, const std::string& charset) {
    auto pos = charset.find(c);
    if (pos == std::string::npos) {
        throw std::runtime_error("Invalid symbol in encoded stream");
    }
    return pos;
}

std::size_t log2_floor(std::size_t n) {
    std::size_t p = 0;
    while ((static_cast<std::size_t>(1) << (p + 1)) <= n) {
        ++p;
    }
    return p;
}

std::vector<std::uint8_t> strip_leading_zeroes(const std::vector<std::uint8_t>& v) {
    auto it = std::find_if(v.begin(), v.end(), [](std::uint8_t b) { return b != 0; });
    return std::vector<std::uint8_t>(it, v.end());
}

// Divide big-endian byte vector by radix. num is modified in-place.
std::size_t divide_bigint(std::vector<std::uint8_t>& num, std::size_t radix) {
    std::size_t carry = 0;
    for (auto& byte : num) {
        std::size_t cur = (carry << 8) | byte;
        byte = static_cast<std::uint8_t>(cur / radix);
        carry = cur % radix;
    }
    num = strip_leading_zeroes(num);
    return carry;
}

void multiply_add(std::vector<std::uint8_t>& num, std::size_t radix, std::size_t add) {
    std::size_t carry = add;
    for (auto it = num.rbegin(); it != num.rend(); ++it) {
        std::size_t cur = (*it) * radix + carry;
        *it = static_cast<std::uint8_t>(cur & 0xFF);
        carry = cur >> 8;
    }
    while (carry > 0) {
        num.insert(num.begin(), static_cast<std::uint8_t>(carry & 0xFF));
        carry >>= 8;
    }
}

std::vector<std::uint8_t> bigint_from_bytes(const std::vector<std::uint8_t>& bytes) {
    auto stripped = strip_leading_zeroes(bytes);
    if (stripped.empty()) {
        return {0};
    }
    return stripped;
}

std::vector<std::uint8_t> bigint_to_bytes(std::vector<std::uint8_t> num, std::size_t out_size) {
    std::vector<std::uint8_t> out(out_size, 0);
    for (std::size_t i = 0; i < out_size; ++i) {
        std::size_t remainder = divide_bigint(num, 256);
        out[out_size - 1 - i] = static_cast<std::uint8_t>(remainder);
        if (num.empty()) {
            break;
        }
    }
    return out;
}

std::string encode_block_pow2(const std::uint8_t* data,
                              std::size_t bytes,
                              const Charset& charset,
                              std::size_t output_length) {
    const std::size_t k = charset.bits_per_symbol;
    std::vector<char> digits;
    digits.reserve(output_length);
    std::uint64_t acc = 0;
    std::size_t acc_bits = 0;
    for (std::size_t i = 0; i < bytes; ++i) {
        std::uint8_t b = data[i];
        for (int bit = 7; bit >= 0; --bit) {
            acc = (acc << 1) | ((b >> bit) & 1U);
            ++acc_bits;
            if (acc_bits == k) {
                digits.push_back(charset.symbols[static_cast<std::size_t>(acc)]);
                acc = 0;
                acc_bits = 0;
            }
        }
    }
    if (acc_bits > 0) {
        acc <<= (k - acc_bits);
        digits.push_back(charset.symbols[static_cast<std::size_t>(acc)]);
    }
    while (digits.size() < output_length) {
        digits.insert(digits.begin(), charset.symbols[0]);
    }
    return std::string(digits.begin(), digits.end());
}

std::string encode_block_general(const std::uint8_t* data,
                                 std::size_t bytes,
                                 const Charset& charset,
                                 std::size_t output_length) {
    std::vector<std::uint8_t> num(data, data + bytes);
    num = bigint_from_bytes(num);
    std::vector<char> digits;
    digits.reserve(output_length);
    const std::size_t radix = charset.effective_radix;

    if (num.size() == 1 && num[0] == 0) {
        digits.push_back(charset.symbols[0]);
    } else {
        while (!num.empty()) {
            std::size_t rem = divide_bigint(num, radix);
            digits.push_back(charset.symbols[rem]);
        }
    }
    while (digits.size() < output_length) {
        digits.push_back(charset.symbols[0]);
    }
    std::reverse(digits.begin(), digits.end());
    return std::string(digits.begin(), digits.end());
}

std::vector<std::uint8_t> decode_block_pow2(const std::string& chunk,
                                            std::size_t block_bytes,
                                            const Charset& charset) {
    const std::size_t k = charset.bits_per_symbol;
    std::vector<bool> bits;
    bits.reserve(chunk.size() * k);
    for (char c : chunk) {
        std::size_t idx = index_in_charset(c, charset.symbols);
        for (int bit = static_cast<int>(k) - 1; bit >= 0; --bit) {
            bits.push_back((idx >> bit) & 1U);
        }
    }
    const std::size_t needed_bits = block_bytes * 8;
    if (bits.size() < needed_bits) {
        throw std::runtime_error("Encoded block shorter than expected");
    }
    std::size_t start = bits.size() - needed_bits;
    std::vector<std::uint8_t> out(block_bytes, 0);
    for (std::size_t i = 0; i < needed_bits; ++i) {
        std::size_t bit_index = start + i;
        std::size_t byte_index = i / 8;
        std::size_t bit_offset = 7 - (i % 8);
        out[byte_index] |= static_cast<std::uint8_t>(bits[bit_index] << bit_offset);
    }
    return out;
}

std::vector<std::uint8_t> decode_block_general(const std::string& chunk,
                                               std::size_t block_bytes,
                                               const Charset& charset) {
    const std::size_t radix = charset.effective_radix;
    std::vector<std::uint8_t> num(1, 0);
    for (char c : chunk) {
        std::size_t idx = index_in_charset(c, charset.symbols);
        multiply_add(num, radix, idx);
    }
    return bigint_to_bytes(num, block_bytes);
}

}  // namespace

Charset build_charset(const std::string& raw, bool pow2) {
    std::string unique;
    unique.reserve(raw.size());
    std::vector<bool> seen(256, false);
    for (char c : raw) {
        unsigned char ch = static_cast<unsigned char>(c);
        if (!seen[ch]) {
            seen[ch] = true;
            unique.push_back(static_cast<char>(ch));
        }
    }
    if (unique.size() < 2) {
        throw std::runtime_error("Charset must contain at least 2 unique symbols");
    }

    Charset charset;
    charset.pow2 = pow2;
    charset.radix = unique.size();
    if (pow2) {
        std::size_t pow = log2_floor(charset.radix);
        charset.effective_radix = static_cast<std::size_t>(1) << pow;
        charset.symbols = unique.substr(0, charset.effective_radix);
        charset.bits_per_symbol = pow;
    } else {
        charset.effective_radix = charset.radix;
        charset.symbols = unique;
        charset.bits_per_symbol = 0;
    }
    return charset;
}

std::size_t compute_output_length(std::size_t block_size_bytes, std::size_t radix) {
    double bits = static_cast<double>(block_size_bytes) * 8.0;
    double logv = std::log2(static_cast<double>(radix));
    return static_cast<std::size_t>(std::ceil(bits / logv));
}

void encode_file(const std::string& input_path,
                 const std::string& output_path,
                 const Charset& charset,
                 std::size_t block_size,
                 bool write_header_flag) {
    if (block_size == 0) {
        throw std::runtime_error("Block size must be positive");
    }
    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open input file: " + input_path);
    }
    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open output file: " + output_path);
    }

    std::size_t output_length = compute_output_length(block_size, charset.effective_radix);
    if (write_header_flag) {
        Metadata meta;
        meta.block_size = static_cast<std::uint32_t>(block_size);
        meta.output_length = static_cast<std::uint32_t>(output_length);
        meta.charset_length = static_cast<std::uint32_t>(charset.symbols.size());
        meta.pow2 = charset.pow2;
        meta.charset = charset.symbols;
        write_header(out, meta);
    }

    std::vector<std::uint8_t> buffer(block_size);
    while (in) {
        in.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(block_size));
        std::streamsize got = in.gcount();
        if (got <= 0) {
            break;
        }
        std::string encoded;
        if (charset.pow2) {
            encoded = encode_block_pow2(buffer.data(), static_cast<std::size_t>(got), charset, output_length);
        } else {
            encoded = encode_block_general(buffer.data(), static_cast<std::size_t>(got), charset, output_length);
        }
        out.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
    }
}

void decode_file(const std::string& input_path,
                 const std::string& output_path,
                 const Charset* charset_override,
                 std::size_t block_size_override,
                 bool header_expected) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Cannot open input file: " + input_path);
    }
    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open output file: " + output_path);
    }

    Charset charset;
    std::size_t block_size = 0;
    std::size_t output_length = 0;

    if (header_expected) {
        Metadata meta = read_header(in);
        charset = build_charset(meta.charset, meta.pow2);
        block_size = meta.block_size;
        output_length = meta.output_length;
    } else {
        if (charset_override == nullptr) {
            throw std::runtime_error("Charset required when header is absent");
        }
        charset = *charset_override;
        block_size = block_size_override;
        if (block_size == 0) {
            throw std::runtime_error("Block size required when header is absent");
        }
        output_length = compute_output_length(block_size, charset.effective_radix);
    }

    std::string chunk(output_length, '\0');
    while (in) {
        in.read(chunk.data(), static_cast<std::streamsize>(output_length));
        std::streamsize got = in.gcount();
        if (got == 0) {
            break;
        }
        if (got != static_cast<std::streamsize>(output_length)) {
            throw std::runtime_error("Partial block encountered during decode");
        }
        std::vector<std::uint8_t> decoded;
        if (charset.pow2) {
            decoded = decode_block_pow2(chunk, block_size, charset);
        } else {
            decoded = decode_block_general(chunk, block_size, charset);
        }
        out.write(reinterpret_cast<const char*>(decoded.data()), static_cast<std::streamsize>(decoded.size()));
    }
}

}  // namespace fluxbase
