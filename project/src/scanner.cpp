/**
 * @file scanner.cpp
 * @brief Scan helpers and block utilities for image data.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#include "scanner.hpp"
#include <algorithm>
#include <cstdlib>

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
            b.pixels.reserve(block_size * block_size);
            for (uint32_t row = 0; row < block_size; ++row) {
                uint32_t img_row = br * block_size + row;
                uint32_t img_col_start = bc * block_size;
                for (uint32_t col = 0; col < block_size; ++col) {
                    b.pixels.push_back(pixels[img_row * width + img_col_start + col]);
                }
            }
            blocks.push_back(std::move(b));
        }
    }
    return blocks;
}


