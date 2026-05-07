/**
 * @file lzss.cpp
 * @brief LZSS encoder and decoder.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#include "lzss.hpp"
#include <sstream>
#include <algorithm>

struct Window {
    uint8_t  buf[LZSS_WINDOW_SIZE] = {};
    uint32_t pos = 0;

    void push(uint8_t byte) {
        buf[pos] = byte;
        pos = (pos + 1) % LZSS_WINDOW_SIZE;
    }
};

struct SlidingDictionary {
    static constexpr uint32_t HASH_SIZE = 65536;
    static constexpr uint32_t NIL       = LZSS_WINDOW_SIZE;

    uint32_t head[HASH_SIZE];
    uint32_t prev[LZSS_WINDOW_SIZE];

    SlidingDictionary() {
        std::fill(head, head + HASH_SIZE, NIL);
        std::fill(prev, prev + LZSS_WINDOW_SIZE, NIL);
    }

    /// 2-byte hash of bytes a and b.
    static uint32_t hash2(uint8_t a, uint8_t b) {
        return (static_cast<uint32_t>(a) << 8) | b;
    }

    /// Insert window slot `slot` with 2-byte key (a,b).
    void insert(uint32_t slot, uint8_t a, uint8_t b) {
        uint32_t h   = hash2(a, b);
        prev[slot]   = head[h];
        head[h]      = slot;
    }

    /// Walk the chain for hash(a,b) and find the longest match for `pattern`.
    /// Returns (best_offset, best_length) or (0, 0) if no match >= MIN_MATCH.
    std::pair<uint32_t,uint32_t> find_match(
            const uint8_t* pattern, uint32_t pat_len,
            const Window& win) const {
        if (pat_len < LZSS_MIN_MATCH) return {0, 0};

        uint32_t h    = hash2(pattern[0], pattern[1]);
        uint32_t slot = head[h];
        uint32_t best_len = 0;
        uint32_t best_off = 0;
        int      chain_limit = 128;

        while (slot != NIL && chain_limit-- > 0) {
            // Compute the distance (offset) from current input_pos to this slot.
            uint32_t distance = (win.pos + LZSS_WINDOW_SIZE - slot) % LZSS_WINDOW_SIZE;
            if (distance == 0 || distance > LZSS_WINDOW_SIZE) {
                slot = prev[slot];
                continue;
            }

            // Match the pattern against the window starting at `slot`.
            uint32_t match_len = 0;
            uint32_t max_len   = std::min(pat_len, LZSS_LOOKAHEAD);
            while (match_len < max_len) {
                uint32_t effective_k = (match_len < distance) ? match_len
                                                               : (match_len % distance);
                uint32_t win_idx = (slot + effective_k) % LZSS_WINDOW_SIZE;
                if (win.buf[win_idx] != pattern[match_len]) break;
                ++match_len;
            }

            if (match_len > best_len) {
                best_len = match_len;
                best_off = distance;
                if (best_len == LZSS_LOOKAHEAD) break;
            }

            slot = prev[slot];
        }

        if (best_len < LZSS_MIN_MATCH) return {0, 0};
        return {best_off, best_len};
    }
};

void lzss_encode(const std::vector<uint8_t>& input, BitWriter& out) {
    Window            win;
    SlidingDictionary dict;

    size_t pos = 0;
    const size_t total = input.size();

    while (pos < total) {
        uint32_t remaining = static_cast<uint32_t>(total - pos);
        uint32_t look_len  = std::min(remaining, LZSS_LOOKAHEAD);

        auto [offset, length] = dict.find_match(input.data() + pos, look_len, win);

        if (length >= LZSS_MIN_MATCH) {
            out.write_bits(1, 1);
            out.write_bits(offset, LZSS_OFFSET_BITS);
            out.write_bits(length - LZSS_MIN_MATCH, LZSS_LENGTH_BITS);
            for (uint32_t k = 0; k < length; ++k) {
                uint8_t byte = input[pos + k];
                if (pos + k + 1 < total)
                    dict.insert(win.pos, byte, input[pos + k + 1]);
                win.push(byte);
            }
            pos += length;
        } else {
            out.write_bits(0, 1);
            out.write_bits(input[pos], 8);
            if (pos + 1 < total)
                dict.insert(win.pos, input[pos], input[pos + 1]);
            win.push(input[pos]);
            ++pos;
        }
    }
}

std::vector<uint8_t> lzss_encode_to_buffer(const std::vector<uint8_t>& input) {
    std::ostringstream oss(std::ios::binary);
    {
        BitWriter bw(oss);
        lzss_encode(input, bw);
        bw.flush();
    }
    const std::string& s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

void lzss_decode(BitReader& in, std::vector<uint8_t>& output, size_t expected_size) {
    output.clear();
    output.reserve(expected_size);

    Window win;

    while (output.size() < expected_size) {
        bool is_match = in.read_bits(1) != 0;

        if (!is_match) {
            uint8_t byte = static_cast<uint8_t>(in.read_bits(8));
            output.push_back(byte);
            win.push(byte);
        } else {
            uint32_t offset = in.read_bits(LZSS_OFFSET_BITS);
            uint32_t length = in.read_bits(LZSS_LENGTH_BITS) + LZSS_MIN_MATCH;

            uint32_t start = (win.pos + LZSS_WINDOW_SIZE - offset) % LZSS_WINDOW_SIZE;
            for (uint32_t k = 0; k < length && output.size() < expected_size; ++k) {
                uint8_t byte = win.buf[(start + k) % LZSS_WINDOW_SIZE];
                output.push_back(byte);
                win.push(byte);
            }
        }
    }
}
