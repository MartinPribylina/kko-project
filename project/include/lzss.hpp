/**
 * @file     lzss.hpp
 * @brief    LZSS (Lempel-Ziv-Storer-Szymanski) encoder and decoder.
 *           Uses a 4 KB sliding window dictionary and groups flags 8 per byte.
 * @author   Martin Pribylina (xpriby19)
 * @date     2026-05-06
 */

#pragma once

#include <cstdint>
#include <vector>
#include "bitio.hpp"

/* -------------------------------------------------------------------------
 * LZSS parameters (compile-time constants)
 * ---------------------------------------------------------------------- */

/// Size of the sliding window / search buffer in bytes.
/// A 12-bit offset field addresses positions 1..LZSS_WINDOW_SIZE.
constexpr uint32_t LZSS_WINDOW_SIZE = 4096;

/// Maximum number of bytes that can be matched in one look-ahead.
/// A 5-bit length field stores (match_length - LZSS_MIN_MATCH).
constexpr uint32_t LZSS_LOOKAHEAD   = 32;

/// Minimum match length worth encoding as a back-reference.
/// Shorter matches are cheaper as literals.
constexpr uint32_t LZSS_MIN_MATCH   = 3;

/// Number of bits in the offset field.
constexpr uint32_t LZSS_OFFSET_BITS = 12;

/// Number of bits in the length field (encodes length - LZSS_MIN_MATCH).
constexpr uint32_t LZSS_LENGTH_BITS = 5;

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

/**
 * Encode a byte buffer with LZSS and write the compressed bit-stream.
 *
 * Flags are grouped 8 per flag-byte: the flag byte is written first,
 * followed by the payloads (literal bytes or offset+length pairs) for
 * each of the 8 positions.
 *
 * @param input  Bytes to compress.
 * @param out    BitWriter connected to the output stream.
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
