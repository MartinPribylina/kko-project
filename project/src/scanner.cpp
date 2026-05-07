/**
 * @file scanner.cpp
 * @brief Scan helpers and block utilities for image data.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#include "scanner.hpp"
#include <algorithm>
#include <cstdlib>

std::vector<uint8_t> scan_horizontal(const std::vector<uint8_t>& pixels,
                                     uint32_t width, uint32_t height) {
    // Row-major order is the native storage layout.height
    return pixels;
}

std::vector<uint8_t> scan_vertical(const std::vector<uint8_t>& pixels,
                                   uint32_t width, uint32_t height) {
    std::vector<uint8_t> out;
    out.reserve(pixels.size());
    // For each column, emit all rows from top to bottom.
    for (uint32_t col = 0; col < width; ++col) {
        for (uint32_t row = 0; row < height; ++row) {
            out.push_back(pixels[row * width + col]);
        }
    }
    return out;
}

std::vector<uint8_t> unscan_horizontal(const std::vector<uint8_t>& data,
                                       uint32_t /*width*/, uint32_t /*height*/) {
    return data;
}

std::vector<uint8_t> unscan_vertical(const std::vector<uint8_t>& data,
                                     uint32_t width, uint32_t height) {
    std::vector<uint8_t> result(width * height, 0);
    for (uint32_t col = 0; col < width; ++col) {
        for (uint32_t row = 0; row < height; ++row) {
            result[row * width + col] = data[col * height + row];
        }
    }
    return result;
}

std::vector<Block> split_into_blocks(const std::vector<uint8_t>& pixels,
                                     uint32_t width, uint32_t height,
                                     uint32_t block_size) {
    std::vector<Block> blocks;

    uint32_t blocks_x = (width  + block_size - 1) / block_size;
    uint32_t blocks_y = (height + block_size - 1) / block_size;
    blocks.reserve(blocks_x * blocks_y);

    for (uint32_t br = 0; br < blocks_y; ++br) {
        for (uint32_t bc = 0; bc < blocks_x; ++bc) {
            Block b;
            b.block_row = br;
            b.block_col = bc;
            b.width  = std::min(block_size, width  - bc * block_size);
            b.height = std::min(block_size, height - br * block_size);
            b.meta   = {0, 1};

            b.pixels.reserve(b.width * b.height);
            for (uint32_t row = 0; row < b.height; ++row) {
                uint32_t img_row = br * block_size + row;
                uint32_t img_col_start = bc * block_size;
                for (uint32_t col = 0; col < b.width; ++col) {
                    b.pixels.push_back(pixels[img_row * width + img_col_start + col]);
                }
            }
            blocks.push_back(std::move(b));
        }
    }
    return blocks;
}

std::vector<uint8_t> merge_blocks(const std::vector<Block>& blocks,
                                  uint32_t width, uint32_t height,
                                  uint32_t block_size) {
    std::vector<uint8_t> result(width * height, 0);

    for (const Block& b : blocks) {
        std::vector<uint8_t> row_major;
        if (b.meta.scan_dir == 0) {
            row_major = b.pixels;
        } else {
            row_major = unscan_vertical(b.pixels, b.width, b.height);
        }

        for (uint32_t row = 0; row < b.height; ++row) {
            uint32_t img_row = b.block_row * block_size + row;
            uint32_t img_col_start = b.block_col * block_size;
            for (uint32_t col = 0; col < b.width; ++col) {
                result[img_row * width + img_col_start + col] = row_major[row * b.width + col];
            }
        }
    }
    return result;
}

uint32_t compute_sad_horizontal(const std::vector<uint8_t>& block_pixels,
                                uint32_t bw, uint32_t bh) {
    uint32_t sad = 0;
    for (uint32_t row = 0; row < bh; ++row) {
        for (uint32_t col = 1; col < bw; ++col) {
            int diff = static_cast<int>(block_pixels[row * bw + col])
                     - static_cast<int>(block_pixels[row * bw + col - 1]);
            sad += static_cast<uint32_t>(std::abs(diff));
        }
    }
    return sad;
}

uint32_t compute_sad_vertical(const std::vector<uint8_t>& block_pixels,
                              uint32_t bw, uint32_t bh) {
    uint32_t sad = 0;
    for (uint32_t row = 1; row < bh; ++row) {
        for (uint32_t col = 0; col < bw; ++col) {
            int diff = static_cast<int>(block_pixels[row * bw + col])
                     - static_cast<int>(block_pixels[(row - 1) * bw + col]);
            sad += static_cast<uint32_t>(std::abs(diff));
        }
    }
    return sad;
}

uint8_t select_scan_dir(const std::vector<uint8_t>& block_pixels,
                        uint32_t bw, uint32_t bh) {
    uint32_t sad_h = compute_sad_horizontal(block_pixels, bw, bh);
    uint32_t sad_v = compute_sad_vertical(block_pixels, bw, bh);
    return (sad_h <= sad_v) ? 0u : 1u;
}
