/**
 * @file     codec.hpp
 * @brief    Public interface for the LZSS compress/decompress pipeline.
 * @author   Martin Pribylina (xpriby19)
 * @date     2026-05-06
 */

#pragma once

#include <iostream>
#include <cstdint>

/**
 * Compress a RAW grayscale image.
 * @param in         Open binary input stream (RAW pixel data).
 * @param out        Open binary output stream (LZKO file).
 * @param width      Image width in pixels.
 * @param use_model  Apply delta model before encoding.
 * @param adaptive   Use per-block adaptive scan direction.
 * @return 0 on success, non-zero on error.
 */
int compress(std::istream& in, std::ostream& out,
             int width, bool use_model, bool adaptive);

/**
 * Decompress an LZKO file.
 * @param in   Open binary input stream (LZKO file).
 * @param out  Open binary output stream (RAW pixel data).
 * @return 0 on success, non-zero on error.
 */
int decompress(std::istream& in, std::ostream& out);
