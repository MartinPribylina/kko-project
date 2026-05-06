/**
 * @file     scanner.hpp
 * @brief    Image serialization — converts 2D grayscale images to 1D byte
 *           streams via horizontal or vertical scan, with block splitting
 *           and a SAD-based heuristic for adaptive scan direction selection.
 * @author   Martin Pribylina (xpriby19)
 * @date     2026-05-06
 */

#pragma once

#include <cstdint>
#include <vector>

/* -------------------------------------------------------------------------
 * Data structures
 * ---------------------------------------------------------------------- */

/**
 * Metadata associated with a single image block.
 */
struct BlockMeta {
    uint8_t scan_dir;    ///< Scan direction chosen for this block: 0=horizontal, 1=vertical
    uint8_t compressed;  ///< Compression status: 1=LZSS compressed, 0=stored raw
};

/**
 * One fixed-size (or partial edge) block extracted from the image.
 */
struct Block {
    std::vector<uint8_t> pixels;  ///< Block pixels in row-major order (original 2D layout)
    uint32_t block_row;           ///< Block row index within the image grid
    uint32_t block_col;           ///< Block column index within the image grid
    uint32_t width;               ///< Actual block width  (may be < block_size at right edge)
    uint32_t height;              ///< Actual block height (may be < block_size at bottom edge)
    BlockMeta meta;               ///< Scan and compression metadata (set during compression)
};

/* -------------------------------------------------------------------------
 * Full-image scan functions (static mode)
 * ---------------------------------------------------------------------- */

/**
 * Horizontal (row-major) scan of the entire image.
 * Returns a copy of the pixel buffer — the input is already stored row-major.
 *
 * @param pixels  Image pixels in row-major order (pixels[row*width+col]).
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 */
std::vector<uint8_t> scan_horizontal(const std::vector<uint8_t>& pixels,
                                     uint32_t width, uint32_t height);

/**
 * Vertical (column-major) scan of the entire image.
 * Returns pixels column by column: for each column, all rows from top to bottom.
 *
 * @param pixels  Image pixels in row-major order.
 * @param width   Image width in pixels.
 * @param height  Image height in pixels.
 */
std::vector<uint8_t> scan_vertical(const std::vector<uint8_t>& pixels,
                                   uint32_t width, uint32_t height);

/**
 * Reconstruct 2D row-major image from a horizontal scan (identity).
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

/* -------------------------------------------------------------------------
 * Block-level functions (adaptive mode)
 * ---------------------------------------------------------------------- */

/**
 * Split an image into fixed-size blocks.
 * Edge blocks at the right and bottom boundary may be smaller than block_size.
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
 * Uses block.meta.scan_dir to determine the scan order of each block's pixel data.
 * Block.pixels must already contain the scanned (1D) representation.
 *
 * @param blocks      Blocks produced by split_into_blocks (meta.scan_dir must be set).
 * @param width       Image width.
 * @param height      Image height.
 * @param block_size  Block size used during splitting.
 */
std::vector<uint8_t> merge_blocks(const std::vector<Block>& blocks,
                                  uint32_t width, uint32_t height,
                                  uint32_t block_size);

/* -------------------------------------------------------------------------
 * Heuristic functions for adaptive scan direction selection
 * ---------------------------------------------------------------------- */

/**
 * Sum of absolute differences between horizontally adjacent pixels (row-direction).
 * Lower value → smoother in the horizontal direction → better with horizontal scan.
 *
 * @param block_pixels  Block pixels in row-major order.
 * @param bw            Block width.
 * @param bh            Block height.
 */
uint32_t compute_sad_horizontal(const std::vector<uint8_t>& block_pixels,
                                uint32_t bw, uint32_t bh);

/**
 * Sum of absolute differences between vertically adjacent pixels (column-direction).
 * Lower value → smoother in the vertical direction → better with vertical scan.
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
