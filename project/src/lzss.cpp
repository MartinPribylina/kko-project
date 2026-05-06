/**
 * @file     lzss.cpp
 * @brief    LZSS encoder and decoder implementation.
 *
 *           Encoder uses a hash-chain approach for O(1) average-case
 *           match search: a hash table keyed on 2-byte pairs provides the
 *           head of each chain; prev[] links earlier occurrences.
 *
 *           Flags are grouped 8 per flag-byte.  The encoder buffers up to 8
 *           (flag, payload) pairs, then writes: flag-byte, payload-byte(s) * 8.
 *
 *           Decoder reads flag bytes and interprets each bit as literal/match.
 *           It stops after exactly expected_size bytes are reconstructed.
 *
 * @author   Martin Pribylina (xpriby19)
 * @date     2026-05-06
 */

#include "lzss.hpp"
#include <sstream>
#include <cstring>
#include <algorithm>

/* =========================================================================
 * Internal helpers
 * ====================================================================== */

namespace {

/// Circular sliding window used by both encoder and decoder.
struct Window {
    uint8_t  buf[LZSS_WINDOW_SIZE] = {};  ///< The window bytes
    uint32_t pos = 0;                     ///< Next write position (mod WINDOW_SIZE)

    /// Append one byte to the window and advance pos.
    void push(uint8_t byte) {
        buf[pos] = byte;
        pos = (pos + 1) % LZSS_WINDOW_SIZE;
    }
};

/**
 * Flag-group buffer used during encoding.
 * Collects 8 (is_match, payload_bytes, payload_len) records, then flushes
 * them as: 1 flag-byte + concatenated payloads.
 */
struct FlagGroup {
    static constexpr int GROUP  = 8;
    uint8_t flags[GROUP]  = {};    ///< 1 = match, 0 = literal
    // Payload bytes: either 1 byte (literal) or 3 bytes (12-bit offset + 5-bit length packed)
    // We store them pre-packed as bit values.
    uint32_t offsets[GROUP] = {};
    uint32_t lengths[GROUP] = {};
    uint8_t  literals[GROUP] = {};
    int count = 0;                ///< Number of items in current group

    /// Add a literal.
    void add_literal(uint8_t byte) {
        flags[count]    = 0;
        literals[count] = byte;
        ++count;
    }

    /// Add a back-reference.
    void add_match(uint32_t offset, uint32_t length) {
        flags[count]   = 1;
        offsets[count] = offset;
        lengths[count] = length - LZSS_MIN_MATCH;  // encode relative to MIN_MATCH
        ++count;
    }

    bool full() const { return count == GROUP; }

    /// Flush all buffered items to a BitWriter.
    void flush(BitWriter& out) {
        if (count == 0) return;
        // Write flag byte: bit 7 = item 0, bit 6 = item 1, …, bit (8-count) = item (count-1)
        uint8_t flag_byte = 0;
        for (int i = 0; i < count; ++i) {
            flag_byte = static_cast<uint8_t>((flag_byte << 1) | flags[i]);
        }
        // Pad remaining slots with 0 flags (literal slots that won't be read).
        for (int i = count; i < GROUP; ++i) {
            flag_byte = static_cast<uint8_t>(flag_byte << 1);
        }
        out.write_bits(flag_byte, 8);

        // Write payloads in order.
        for (int i = 0; i < count; ++i) {
            if (flags[i] == 0) {
                out.write_bits(literals[i], 8);
            } else {
                out.write_bits(offsets[i], LZSS_OFFSET_BITS);
                out.write_bits(lengths[i], LZSS_LENGTH_BITS);
            }
        }
        count = 0;
    }
};

/**
 * Hash-chain search structure for LZSS encoder.
 *
 * head[h] holds the most recent window slot whose 2-byte hash equals h.
 * prev[slot] holds the previous slot in the same chain.
 * A value of LZSS_WINDOW_SIZE means "end of chain".
 */
struct HashChains {
    static constexpr uint32_t HASH_SIZE = 65536;
    static constexpr uint32_t NIL       = LZSS_WINDOW_SIZE;

    uint32_t head[HASH_SIZE];
    uint32_t prev[LZSS_WINDOW_SIZE];

    HashChains() {
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
        int      chain_limit = 128;  // cap chain walk length for speed

        while (slot != NIL && chain_limit-- > 0) {
            // Compute the distance (offset) from current input_pos to this slot.
            // window pos `slot` was written at some prior time.
            // offset = (win.pos - slot + WINDOW_SIZE) % WINDOW_SIZE
            // (win.pos is where the NEXT byte will be written)
            uint32_t distance = (win.pos + LZSS_WINDOW_SIZE - slot) % LZSS_WINDOW_SIZE;
            if (distance == 0 || distance > LZSS_WINDOW_SIZE) {
                slot = prev[slot];
                continue;
            }

            // Match the pattern against the window starting at `slot`.
            // For overlapping copies (match_len >= distance) simulate the decoder:
            // the decoder reads from slot + (k % distance) cyclically, not from
            // uninitialized window bytes beyond slot+distance.
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
                if (best_len == LZSS_LOOKAHEAD) break;  // can't do better
            }

            slot = prev[slot];
        }

        if (best_len < LZSS_MIN_MATCH) return {0, 0};
        return {best_off, best_len};
    }
};

/* =========================================================================
 * Core encode implementation (shared by lzss_encode and lzss_encode_to_buffer)
 * ====================================================================== */
void encode_impl(const std::vector<uint8_t>& input, BitWriter& out) {
    Window     win;
    HashChains chains;
    FlagGroup  group;

    size_t pos = 0;
    const size_t total = input.size();

    while (pos < total) {
        uint32_t remaining = static_cast<uint32_t>(total - pos);
        uint32_t look_len  = std::min(remaining, LZSS_LOOKAHEAD);

        auto [offset, length] = chains.find_match(
                input.data() + pos, look_len, win);

        if (length >= LZSS_MIN_MATCH) {
            // Emit a back-reference.
            group.add_match(offset, length);
            // Advance the window by `length` bytes.
            for (uint32_t k = 0; k < length; ++k) {
                uint8_t byte = input[pos + k];
                // Insert into hash chains if there are at least 2 bytes ahead.
                if (pos + k + 1 < total) {
                    chains.insert(win.pos, byte, input[pos + k + 1]);
                }
                win.push(byte);
            }
            pos += length;
        } else {
            // Emit a literal.
            uint8_t byte = input[pos];
            group.add_literal(byte);
            if (pos + 1 < total) {
                chains.insert(win.pos, byte, input[pos + 1]);
            }
            win.push(byte);
            ++pos;
        }

        if (group.full()) {
            group.flush(out);
        }
    }

    // Flush the remaining partial group (if any).
    group.flush(out);
}

} // anonymous namespace

/* =========================================================================
 * Public API
 * ====================================================================== */

void lzss_encode(const std::vector<uint8_t>& input, BitWriter& out) {
    encode_impl(input, out);
}

std::vector<uint8_t> lzss_encode_to_buffer(const std::vector<uint8_t>& input) {
    std::ostringstream oss(std::ios::binary);
    {
        BitWriter bw(oss);
        encode_impl(input, bw);
        bw.flush();
    }
    const std::string& s = oss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

void lzss_decode(BitReader& in, std::vector<uint8_t>& output, size_t expected_size) {
    output.clear();
    output.reserve(expected_size);

    Window win;

    static constexpr int GROUP = 8;

    while (output.size() < expected_size) {
        size_t remaining   = expected_size - output.size();
        int    group_count = static_cast<int>(std::min(remaining, static_cast<size_t>(GROUP)));

        // Read the flag byte.
        uint8_t flag_byte = static_cast<uint8_t>(in.read_bits(8));

        for (int i = 0; i < group_count && output.size() < expected_size; ++i) {
            bool is_match = (flag_byte >> (7 - i)) & 1u;

            if (!is_match) {
                // Literal byte.
                uint8_t byte = static_cast<uint8_t>(in.read_bits(8));
                output.push_back(byte);
                win.push(byte);
            } else {
                // Back-reference: read offset and encoded length.
                uint32_t offset = in.read_bits(LZSS_OFFSET_BITS);  // 1-based distance
                uint32_t length = in.read_bits(LZSS_LENGTH_BITS) + LZSS_MIN_MATCH;

                // Copy `length` bytes from the window, one at a time.
                // The start position in the circular buffer is win.pos - offset (mod WINDOW_SIZE).
                uint32_t start = (win.pos + LZSS_WINDOW_SIZE - offset) % LZSS_WINDOW_SIZE;
                for (uint32_t k = 0; k < length && output.size() < expected_size; ++k) {
                    // Read one byte at a time to handle overlapping copies correctly.
                    uint8_t byte = win.buf[(start + k) % LZSS_WINDOW_SIZE];
                    output.push_back(byte);
                    win.push(byte);
                }
            }
        }
    }
}
