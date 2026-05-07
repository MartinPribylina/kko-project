/**
 * @file codec.hpp
 * @brief Public interface for compression and decompression.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#pragma once

#include <iostream>
#include <cstdint>

/**
 * Compress a RAW grayscale image.
 * @param in         Open binary input stream.
 * @param out        Open binary output stream.
 * @param width      Image width in pixels.
 * @param use_model  Apply delta model before encoding.
 * @param adaptive   Use per-block adaptive scan direction.
 * @return 0 on success, non-zero on error.
 */
int compress(std::istream& in, std::ostream& out,
             int width, bool use_model, bool adaptive);

/**
 * Decompress an LZKO file.
 * @param in   Open binary input stream.
 * @param out  Open binary output stream.
 * @return 0 on success, non-zero on error.
 */
int decompress(std::istream& in, std::ostream& out);
