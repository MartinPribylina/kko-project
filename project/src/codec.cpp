/**
 * @file codec.cpp
 * @brief Compression and decompression for the LZKO format.
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


static std::vector<uint8_t> pack_block_meta(const std::vector<Block>& blocks) {
    uint32_t n    = static_cast<uint32_t>(blocks.size());
    std::vector<uint8_t> buf((n * 2 + 7) / 8, 0);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t bp = i * 2;
        if (blocks[i].meta.scan_dir)   buf[bp / 8]       |= static_cast<uint8_t>(1u << (7 - bp % 8));
        if (blocks[i].meta.compressed) buf[(bp+1) / 8]   |= static_cast<uint8_t>(1u << (7 - (bp+1) % 8));
    }
    return buf;
}

static void unpack_block_meta(const std::vector<uint8_t>& buf, uint32_t n,
                              std::vector<uint8_t>& scan_dirs,
                              std::vector<uint8_t>& comp_flags) {
    scan_dirs.resize(n);
    comp_flags.resize(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t bp = i * 2;
        scan_dirs[i]  = (buf[bp / 8]     >> (7 - bp % 8))     & 1u;
        comp_flags[i] = (buf[(bp+1) / 8] >> (7 - (bp+1) % 8)) & 1u;
    }
}

static constexpr uint32_t BLOCK_SIZE     = 16;
static constexpr uint8_t  BLOCK_SIZE_LOG = 4;

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

    const uint8_t magic[4] = {0x4C, 0x5A, 0x4B, 0x4F};
    out.write(reinterpret_cast<const char*>(magic), 4);
    write_u16(out, static_cast<uint16_t>(image_width));
    write_u16(out, static_cast<uint16_t>(image_height));
    out.put(static_cast<char>((use_model ? 1u : 0u) | (adaptive ? 2u : 0u)));
    out.put(static_cast<char>(BLOCK_SIZE_LOG));
    write_u32(out, static_cast<uint32_t>(pixels.size()));

    if (!adaptive) {
        std::vector<uint8_t> data = scan_horizontal(pixels, image_width, image_height);
        if (use_model) forward_model(data);
        BitWriter bw(out);
        lzss_encode(data, bw);
        bw.flush();
    } else {
        std::vector<Block> blocks = split_into_blocks(pixels, image_width, image_height, BLOCK_SIZE);

        for (Block& b : blocks) {
            b.meta.scan_dir = select_scan_dir(b.pixels, b.width, b.height);
            std::vector<uint8_t> scanned = (b.meta.scan_dir == 0)
                ? scan_horizontal(b.pixels, b.width, b.height)
                : scan_vertical(b.pixels, b.width, b.height);
            if (use_model) forward_model(scanned);
            std::vector<uint8_t> encoded = lzss_encode_to_buffer(scanned);
            b.meta.compressed = (encoded.size() < scanned.size()) ? 1u : 0u;
        }

        uint32_t num_blocks = static_cast<uint32_t>(blocks.size());
        write_u32(out, num_blocks);
        std::vector<uint8_t> meta = pack_block_meta(blocks);
        out.write(reinterpret_cast<const char*>(meta.data()),
                  static_cast<std::streamsize>(meta.size()));

        for (const Block& b : blocks) {
            std::vector<uint8_t> scanned = (b.meta.scan_dir == 0)
                ? scan_horizontal(b.pixels, b.width, b.height)
                : scan_vertical(b.pixels, b.width, b.height);
            if (use_model) forward_model(scanned);

            if (b.meta.compressed) {
                std::vector<uint8_t> encoded = lzss_encode_to_buffer(scanned);
                write_u32(out, static_cast<uint32_t>(encoded.size()));
                out.write(reinterpret_cast<const char*>(encoded.data()),
                          static_cast<std::streamsize>(encoded.size()));
            } else {
                write_u32(out, static_cast<uint32_t>(scanned.size()));
                out.write(reinterpret_cast<const char*>(scanned.data()),
                          static_cast<std::streamsize>(scanned.size()));
            }
        }
    }

    return 0;
}

int decompress(std::istream& in, std::ostream& out) {
    uint8_t magic[4];
    in.read(reinterpret_cast<char*>(magic), 4);
    if (magic[0] != 0x4C || magic[1] != 0x5A || magic[2] != 0x4B || magic[3] != 0x4F) {
        std::cerr << "Neplatný formát souboru (chybné magic bytes)\n";
        return 1;
    }

    uint32_t width         = read_u16(in);
    uint32_t height        = read_u16(in);
    uint8_t  flags         = static_cast<uint8_t>(in.get());
    uint8_t  block_log     = static_cast<uint8_t>(in.get());
    uint32_t block_size    = 1u << block_log;
    bool     use_model     = (flags & 1u) != 0;
    bool     adaptive      = (flags & 2u) != 0;
    uint32_t original_size = read_u32(in);

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

        std::vector<uint8_t> scan_dirs, comp_flags;
        unpack_block_meta(meta_buf, num_blocks, scan_dirs, comp_flags);

        uint32_t blocks_x = (width + block_size - 1) / block_size;
        std::vector<uint8_t> image(width * height, 0);

        for (uint32_t idx = 0; idx < num_blocks; ++idx) {
            uint32_t block_row = idx / blocks_x;
            uint32_t block_col = idx % blocks_x;
            uint32_t bw        = std::min(block_size, width  - block_col * block_size);
            uint32_t bh        = std::min(block_size, height - block_row * block_size);

            uint32_t stored_size = read_u32(in);
            std::vector<uint8_t> block_raw(stored_size);
            in.read(reinterpret_cast<char*>(block_raw.data()),
                    static_cast<std::streamsize>(stored_size));

            std::vector<uint8_t> data;
            if (comp_flags[idx]) {
                std::string s(block_raw.begin(), block_raw.end());
                std::istringstream iss(s, std::ios::binary);
                BitReader brdr(iss);
                lzss_decode(brdr, data, bw * bh);
            } else {
                data = block_raw;
            }

            if (use_model) inverse_model(data);

            std::vector<uint8_t> row_major = (scan_dirs[idx] == 0)
                ? data
                : unscan_vertical(data, bw, bh);

            for (uint32_t row = 0; row < bh; ++row) {
                uint32_t image_col = block_col * block_size;
                for (uint32_t col = 0; col < bw; ++col)
                    image[(block_row * block_size + row) * width + image_col + col] =
                        row_major[row * bw + col];
            }
        }

        out.write(reinterpret_cast<const char*>(image.data()),
                  static_cast<std::streamsize>(image.size()));
    }

    return 0;
}
