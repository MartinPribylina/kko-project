/**
 * @file model.cpp
 * @brief Simple delta model for grayscale data.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#include "model.hpp"

void forward_model(std::vector<uint8_t>& data) {
    if (data.size() < 2) return;
    for (size_t i = data.size() - 1; i > 0; --i) {
        data[i] = static_cast<uint8_t>(data[i] - data[i - 1]);
    }
}

void inverse_model(std::vector<uint8_t>& data) {
    if (data.size() < 2) return;
    for (size_t i = 1; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(data[i] + data[i - 1]);
    }
}
