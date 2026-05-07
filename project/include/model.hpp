/**
 * @file model.hpp
 * @brief Simple delta model for grayscale data.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#pragma once

#include <cstdint>
#include <vector>

/**
 * Apply first-order delta coding.
 *
 * @param data  Byte buffer to transform.
 */
void forward_model(std::vector<uint8_t>& data);

/**
 * Reverse the delta coding transform.
 *
 * @param data  Delta-coded buffer to reconstruct.
 */
void inverse_model(std::vector<uint8_t>& data);
