/**
 * @file     model.cpp
 * @brief    Implementation of forward and inverse pixel difference model.
 * @author   Martin Pribylina (xpriby19)
 * @date     2026-05-06
 */

#include "model.hpp"

void forward_model(std::vector<uint8_t>& data) {
    if (data.size() < 2) return;
    // Iterate backwards so data[i-1] is still the original value when used.
    for (size_t i = data.size() - 1; i >= 1; --i) {
        data[i] = static_cast<uint8_t>(data[i] - data[i - 1]);
    }
    // data[0] unchanged — it is the anchor (original first pixel).
}

void inverse_model(std::vector<uint8_t>& data) {
    if (data.size() < 2) return;
    // Iterate forwards so data[i-1] holds the reconstructed predecessor.
    for (size_t i = 1; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(data[i] + data[i - 1]);
    }
}
