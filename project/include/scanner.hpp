/**
 * @file scanner.hpp
 * @brief Scan helpers and block utilities for image data.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#pragma once

#include <cstdint>
#include <vector>

// One fixed-size block extracted from the image.
// flags bit 0: scan direction (0 = horizontal, 1 = vertical)
// flags bit 1: block was LZSS-compressed (0 = stored raw)
struct Block {
    std::vector<uint8_t> pixels;
    uint8_t flags = 0;
};

/**
 * Vertical scan of the entire image.
 * Returns pixels column by column: for each column, all rows from top to bottom.
 *
 * @param pixels  Image pixels in row order.
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 */
std::vector<uint8_t> scan_vertical(const std::vector<uint8_t>& pixels,
                                   uint32_t width, uint32_t height);

/**
 * Reconstruct 2D row-major image from a vertical (column-major) scan.
 *
 * @param data    Column-major byte stream.
 * @param width   Image width.
 * @param height  Image height.
 */
std::vector<uint8_t> unscan_vertical(const std::vector<uint8_t>& data,
                                     uint32_t width, uint32_t height);

/**
 * Split an image into fixed-size blocks.
 *
 * @param pixels      Full image pixels in row-major order.
 * @param width       Image width.
 * @param height      Image height.
 * @param block_size  Block width and height in pixels (e.g. 16).
 * @return            Vector of Block objects in raster (row-major) order.
 */
std::vector<Block> split_into_blocks(const std::vector<uint8_t>& pixels,
                                     uint32_t width, uint32_t height,
                                     uint32_t block_size);


