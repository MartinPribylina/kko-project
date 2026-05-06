/**
 * @file     model.hpp
 * @brief    Pixel difference model — forward and inverse delta coding for
 *           grayscale image data. Reduces entropy before LZSS compression.
 * @author   Martin Pribylina (xpriby19)
 * @date     2026-05-06
 */

#pragma once

#include <cstdint>
#include <vector>

/**
 * Apply first-order delta coding in-place.
 *
 * Replaces each element with its difference from the preceding element:
 *   data[i] = data[i] - data[i-1]   (uint8_t wrapping arithmetic, i >= 1)
 *   data[0] is left unchanged (anchor value).
 *
 * The iteration runs from the last element to the second element (reverse
 * order) to avoid overwriting a predecessor before it is used.
 *
 * @param data  Byte buffer to transform in-place.
 */
void forward_model(std::vector<uint8_t>& data);

/**
 * Reverse the delta coding transform in-place.
 *
 * Reconstructs original values from differences:
 *   data[i] = data[i] + data[i-1]   (uint8_t wrapping arithmetic, i >= 1)
 *   data[0] is left unchanged.
 *
 * @param data  Delta-coded buffer to reconstruct in-place.
 */
void inverse_model(std::vector<uint8_t>& data);
