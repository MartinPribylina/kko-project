/**
 * @file     bitio.hpp
 * @brief    Bit-level I/O — BitWriter and BitReader classes for packing
 *           arbitrary-width fields into byte streams.
 * @author   Martin Pribylina (xpriby19)
 * @date     2026-05-06
 */

#pragma once

#include <cstdint>
#include <iostream>

/**
 * BitWriter — writes individual bits or multi-bit values into a byte stream.
 *
 * Bits are accumulated into an internal buffer byte, MSB first.
 * When 8 bits are collected the byte is flushed to the underlying stream.
 * Call flush() before destroying the object or closing the stream to ensure
 * any partially filled byte is written (zero-padded on the low side).
 */
class BitWriter {
public:
    /**
     * @param out  Output stream to write bytes into.
     */
    explicit BitWriter(std::ostream& out)
        : out_(out), buf_(0), bits_in_buf_(0) {}

    /**
     * Write a single bit.
     * @param b  true writes a 1 bit; false writes a 0 bit.
     */
    void write_bit(bool b) {
        // Place the new bit at the next MSB-first position.
        buf_ = static_cast<uint8_t>((buf_ << 1) | (b ? 1 : 0));
        ++bits_in_buf_;
        if (bits_in_buf_ == 8) {
            out_.put(static_cast<char>(buf_));
            buf_ = 0;
            bits_in_buf_ = 0;
        }
    }

    /**
     * Write the n least-significant bits of val, MSB first.
     * @param val  Value whose LSBs are written.
     * @param n    Number of bits to write (1..32).
     */
    void write_bits(uint32_t val, int n) {
        for (int i = n - 1; i >= 0; --i) {
            write_bit((val >> i) & 1u);
        }
    }

    /**
     * Flush any pending bits to the stream, zero-padding the low bits.
     * Must be called before the stream is closed or the object is destroyed.
     */
    void flush() {
        if (bits_in_buf_ > 0) {
            // Shift remaining bits to MSB position (zero-pad low bits).
            buf_ = static_cast<uint8_t>(buf_ << (8 - bits_in_buf_));
            out_.put(static_cast<char>(buf_));
            buf_ = 0;
            bits_in_buf_ = 0;
        }
    }

private:
    std::ostream& out_;    ///< Underlying output stream
    uint8_t       buf_;    ///< Accumulation buffer (at most 7 valid bits)
    int   bits_in_buf_;    ///< Number of valid bits currently in buf_
};

/**
 * BitReader — reads individual bits or multi-bit values from a byte stream.
 *
 * Bytes are read lazily from the stream, one at a time, and bits are
 * served MSB first from each byte.
 */
class BitReader {
public:
    /**
     * @param in  Input stream to read bytes from.
     */
    explicit BitReader(std::istream& in)
        : in_(in), buf_(0), bits_left_(0) {}

    /**
     * Read a single bit.
     * @return true if the next bit is 1, false if 0.
     */
    bool read_bit() {
        if (bits_left_ == 0) {
            int c = in_.get();
            if (c == EOF) return false;
            buf_ = static_cast<uint8_t>(c);
            bits_left_ = 8;
        }
        --bits_left_;
        // Return the next MSB.
        return (buf_ >> bits_left_) & 1u;
    }

    /**
     * Read n bits and return them as the least-significant bits of a uint32_t.
     * Bits are read MSB first and assembled left-to-right.
     * @param n  Number of bits to read (1..32).
     */
    uint32_t read_bits(int n) {
        uint32_t result = 0;
        for (int i = 0; i < n; ++i) {
            result = (result << 1) | (read_bit() ? 1u : 0u);
        }
        return result;
    }

    /**
     * @return true if the underlying stream has reached EOF (no more bytes).
     */
    bool eof() const {
        return in_.peek() == EOF && bits_left_ == 0;
    }

private:
    std::istream& in_;     ///< Underlying input stream
    uint8_t       buf_;    ///< Current byte being served bit by bit
    int   bits_left_;      ///< Number of bits still available in buf_
};
