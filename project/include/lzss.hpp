/**
 * @file lzss.hpp
 * @brief LZSS encoder and decoder.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#pragma once

#include <cstdint>
#include <vector>
#include "bitio.hpp"

// Sliding window size.
constexpr uint32_t LZSS_WINDOW_SIZE = 4096;

// Max match length.
constexpr uint32_t LZSS_LOOKAHEAD   = 32;

// Matches shorter than this stay as literals.
constexpr uint32_t LZSS_MIN_MATCH   = 3;

constexpr uint32_t LZSS_OFFSET_BITS = 12;

constexpr uint32_t LZSS_LENGTH_BITS = 5;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/**
 * Encode a byte buffer with LZSS and write the compressed bit-stream.
 *
 * @param input  Bytes to compress.
 * @param out    BitWriter for the output stream.
 */
void lzss_encode(const std::vector<uint8_t>& input, BitWriter& out);

/**
 * Encode to an in-memory byte buffer (for size comparison in adaptive mode).
 *
 * @param input  Bytes to compress.
 * @return       Compressed bytes including flag bytes and token payloads.
 */
std::vector<uint8_t> lzss_encode_to_buffer(const std::vector<uint8_t>& input);

/**
 * Decode an LZSS bit-stream and reconstruct exactly expected_size bytes.
 *
 * @param in            BitReader connected to the compressed stream.
 * @param output        Destination buffer (will be filled with expected_size bytes).
 * @param expected_size Number of original bytes to reconstruct.
 */
void lzss_decode(BitReader& in, std::vector<uint8_t>& output, size_t expected_size);
