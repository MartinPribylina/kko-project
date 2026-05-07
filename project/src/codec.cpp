/**
 * @file codec.cpp
 * @brief Compression and decompression.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#include "codec.hpp"
#include "bitio.hpp"
#include "lzss.hpp"
#include "scanner.hpp"
#include "model.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <algorithm>

// Helper functions

static void write_u16(std::ostream& out, uint16_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
}

static void write_u32(std::ostream& out, uint32_t v) {
    for (int i = 0; i < 4; ++i)
        out.put(static_cast<char>((v >> (8 * i)) & 0xFF));
}

static uint16_t read_u16(std::istream& in) {
    uint16_t v = 0;
    v |= static_cast<uint16_t>(static_cast<uint8_t>(in.get()));
    v |= static_cast<uint16_t>(static_cast<uint8_t>(in.get())) << 8;
    return v;
}

static uint32_t read_u32(std::istream& in) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= static_cast<uint32_t>(static_cast<uint8_t>(in.get())) << (8 * i);
    return v;
}


// Pack 2 flags bits per block into a byte array (MSB-first).
// Bit 0 of block.flags -> even bit position; bit 1 -> odd bit position.
static std::vector<uint8_t> pack_block_meta(const std::vector<Block>& blocks) {
    uint32_t n = static_cast<uint32_t>(blocks.size());
    std::vector<uint8_t> buf((n * 2 + 7) / 8, 0);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t base = i * 2;
        if (blocks[i].flags & 1u)        // scan_dir
            buf[base / 8]       |= static_cast<uint8_t>(0x80u >> (base % 8));
        if (blocks[i].flags & 2u)        // compressed
            buf[(base+1) / 8]   |= static_cast<uint8_t>(0x80u >> ((base+1) % 8));
    }
    return buf;
}

// Unpack flags back into one uint8_t per block (bit 0 = scan_dir, bit 1 = compressed).
static std::vector<uint8_t> unpack_block_meta(const std::vector<uint8_t>& buf, uint32_t n) {
    std::vector<uint8_t> flags(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t base = i * 2;
        flags[i]  = (buf[base / 8]     & (0x80u >> (base % 8)))     ? 1u : 0u;  // bit 0
        flags[i] |= (buf[(base+1) / 8] & (0x80u >> ((base+1) % 8))) ? 2u : 0u;  // bit 1
    }
    return flags;
}

static constexpr uint32_t BLOCK_SIZE = 16;

int compress(std::istream& in, std::ostream& out,
             int width, bool use_model, bool adaptive) {
    std::vector<uint8_t> pixels((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());

    uint32_t image_width = static_cast<uint32_t>(width);
    if (pixels.empty() || pixels.size() % image_width != 0) {
        std::cerr << "Velikost vstupu " << pixels.size()
                  << " není dělitelná šířkou " << image_width << "\n";
        return 1;
    }
    uint32_t image_height = static_cast<uint32_t>(pixels.size() / image_width);

    // Build payload first so we know whether raw fallback is needed before writing the header.
    bool stored_raw = false;
    std::vector<uint8_t> payload;

    if (!adaptive) {
        std::vector<uint8_t> data = pixels;
        if (use_model) forward_model(data);
        std::vector<uint8_t> encoded = lzss_encode_to_buffer(data);
        if (encoded.size() < data.size()) {
            payload = std::move(encoded);
        } else {
            stored_raw = true;
            payload = pixels;
        }
    } else {
        // Buffer the entire adaptive output so we can compare with raw size.
        std::ostringstream adaptive_buf(std::ios::binary);
        // Buffer the block data output to append after flag bytes.
        std::ostringstream block_data_buf(std::ios::binary);

        std::vector<Block> blocks = split_into_blocks(pixels, image_width, image_height, BLOCK_SIZE);

        for (Block& b : blocks) {
            // Encode both scan directions and pick the smaller result.
            std::vector<uint8_t> h_data = b.pixels;
            if (use_model) forward_model(h_data);
            std::vector<uint8_t> h_enc = lzss_encode_to_buffer(h_data);

            std::vector<uint8_t> v_data = scan_vertical(b.pixels, BLOCK_SIZE, BLOCK_SIZE);
            if (use_model) forward_model(v_data);
            std::vector<uint8_t> v_enc = lzss_encode_to_buffer(v_data);

            bool use_vertical = v_enc.size() < h_enc.size();
            const std::vector<uint8_t>& best_enc = use_vertical ? v_enc : h_enc;

            if (best_enc.size() < h_data.size()) {
                b.flags = (use_vertical ? 1u : 0u) | 2u;  // scan_dir | compressed
                write_u32(block_data_buf, static_cast<uint32_t>(best_enc.size()));
                block_data_buf.write(reinterpret_cast<const char*>(best_enc.data()),
                                     static_cast<std::streamsize>(best_enc.size()));
            } else {
                // Compression didn't help — store the original pixels as-is.
                b.flags = 0u;
                write_u32(block_data_buf, static_cast<uint32_t>(b.pixels.size()));
                block_data_buf.write(reinterpret_cast<const char*>(b.pixels.data()),
                                     static_cast<std::streamsize>(b.pixels.size()));
            }
        }

        uint32_t num_blocks = static_cast<uint32_t>(blocks.size());
        write_u32(adaptive_buf, num_blocks);
        std::vector<uint8_t> meta = pack_block_meta(blocks);
        adaptive_buf.write(reinterpret_cast<const char*>(meta.data()),
                  static_cast<std::streamsize>(meta.size()));

        const std::string block_data = block_data_buf.str();
        adaptive_buf.write(block_data.data(),
                static_cast<std::streamsize>(block_data.size()));

        const std::string s = adaptive_buf.str();
        if (s.size() < pixels.size()) {
            payload.assign(s.begin(), s.end());
        } else {
            stored_raw = true;
            payload = pixels;
        }
    }

    // Write header.
    write_u16(out, static_cast<uint16_t>(image_width));
    write_u16(out, static_cast<uint16_t>(image_height));
    out.put(static_cast<char>((use_model ? 1u : 0u) | (adaptive ? 2u : 0u) | (stored_raw ? 4u : 0u)));
    out.write(reinterpret_cast<const char*>(payload.data()),
              static_cast<std::streamsize>(payload.size()));

    return 0;
}

int decompress(std::istream& in, std::ostream& out) {
    uint32_t width         = read_u16(in);
    uint32_t height        = read_u16(in);
    uint8_t  flags         = static_cast<uint8_t>(in.get());
    bool     use_model     = (flags & 1u) != 0;
    bool     adaptive      = (flags & 2u) != 0;
    bool     stored_raw    = (flags & 4u) != 0;
    uint32_t original_size = width * height;

    if (stored_raw) {
        // Payload is the original raw pixels.
        std::vector<uint8_t> data(original_size);
        in.read(reinterpret_cast<char*>(data.data()),
                static_cast<std::streamsize>(original_size));
        out.write(reinterpret_cast<char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
        return 0;
    }

    if (!adaptive) {
        std::vector<uint8_t> data;
        BitReader br(in);
        lzss_decode(br, data, original_size);
        if (use_model) inverse_model(data);
        out.write(reinterpret_cast<char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    } else {
        uint32_t num_blocks   = read_u32(in);
        uint32_t meta_bytes_n = (num_blocks * 2 + 7) / 8;
        std::vector<uint8_t> meta_buf(meta_bytes_n);
        in.read(reinterpret_cast<char*>(meta_buf.data()),
                static_cast<std::streamsize>(meta_bytes_n));

        std::vector<uint8_t> block_flags = unpack_block_meta(meta_buf, num_blocks);

        uint32_t blocks_x = (width + BLOCK_SIZE - 1) / BLOCK_SIZE;
        std::vector<uint8_t> image(width * height, 0);

        for (uint32_t idx = 0; idx < num_blocks; ++idx) {
            uint32_t block_row = idx / blocks_x;
            uint32_t block_col = idx % blocks_x;

            uint32_t stored_size = read_u32(in);
            std::vector<uint8_t> block_raw(stored_size);
            in.read(reinterpret_cast<char*>(block_raw.data()),
                    static_cast<std::streamsize>(stored_size));

            std::vector<uint8_t> data;
            if (block_flags[idx] & 2u) {  // compressed
                std::string s(block_raw.begin(), block_raw.end());
                std::istringstream iss(s, std::ios::binary);
                BitReader brdr(iss);
                lzss_decode(brdr, data, BLOCK_SIZE * BLOCK_SIZE);
            } else {
                data = block_raw;
            }

            if (use_model) inverse_model(data);

            // Only unscan if the block was compressed (scan dir only applies to compressed blocks).
            std::vector<uint8_t> row_major = (block_flags[idx] & 2u) && (block_flags[idx] & 1u)
                ? unscan_vertical(data, BLOCK_SIZE, BLOCK_SIZE)
                : data;

            for (uint32_t row = 0; row < BLOCK_SIZE; ++row) {
                uint32_t image_col = block_col * BLOCK_SIZE;
                for (uint32_t col = 0; col < BLOCK_SIZE; ++col)
                    image[(block_row * BLOCK_SIZE + row) * width + image_col + col] =
                        row_major[row * BLOCK_SIZE + col];
            }
        }

        out.write(reinterpret_cast<const char*>(image.data()),
                  static_cast<std::streamsize>(image.size()));
    }

    return 0;
}
