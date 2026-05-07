/**
 * @file scanner.hpp
 * @brief Scan helpers and block utilities for image data.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#pragma once

#include <cstdint>
#include <vector>

// Single image block metadata
struct BlockMeta {
    uint8_t scan_dir;
    uint8_t compressed;
};

// One fixed-size block extracted from the image.
struct Block {
    std::vector<uint8_t> pixels;  // Block pixels in row order (original 2D layout)
    uint32_t block_row;           // Block row index within the image grid
    uint32_t block_col;           // Block column index within the image grid
    uint32_t width;               // Actual block width
    uint32_t height;              // Actual block height
    BlockMeta meta;               // Scan and compression metadata
};

/**
 * Horizontal scan of the entire image.
 * Returns a copy of the pixel buffer — the input is already stored row-major.
 *
 * @param pixels  Image pixels in row-major order (pixels[row*width+col]).
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 */
std::vector<uint8_t> scan_horizontal(const std::vector<uint8_t>& pixels,
                                     uint32_t width, uint32_t height);

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
 * Reconstruct 2D row-major image from a horizontal scan.
 *
 * @param data    Row-major byte stream.
 * @param width   Image width.
 * @param height  Image height.
 */
std::vector<uint8_t> unscan_horizontal(const std::vector<uint8_t>& data,
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

/**
 * Reassemble blocks into a full image.
 *
 * @param blocks      Blocks produced by split_into_blocks (meta.scan_dir must be set).
 * @param width       Image width.
 * @param height      Image height.
 * @param block_size  Block size used during splitting.
 */
std::vector<uint8_t> merge_blocks(const std::vector<Block>& blocks,
                                  uint32_t width, uint32_t height,
                                  uint32_t block_size);

/**
 * Sum of absolute differences between horizontally adjacent pixels.
 * Lower value => smoother in the horizontal direction => better with horizontal scan.
 *
 * @param block_pixels  Block pixels in row-major order.
 * @param bw            Block width.
 * @param bh            Block height.
 */
uint32_t compute_sad_horizontal(const std::vector<uint8_t>& block_pixels,
                                uint32_t bw, uint32_t bh);

/**
 * Sum of absolute differences between vertically adjacent pixels.
 * Lower value => smoother in the vertical direction => better with vertical scan.
 *
 * @param block_pixels  Block pixels in row-major order.
 * @param bw            Block width.
 * @param bh            Block height.
 */
uint32_t compute_sad_vertical(const std::vector<uint8_t>& block_pixels,
                              uint32_t bw, uint32_t bh);

/**
 * Select the scan direction with the lower SAD (better compressibility).
 *
 * @param block_pixels  Block pixels in row-major order.
 * @param bw            Block width.
 * @param bh            Block height.
 * @return 0 if horizontal is better or equal, 1 if vertical is better.
 */
uint8_t select_scan_dir(const std::vector<uint8_t>& block_pixels,
                        uint32_t bw, uint32_t bh);
