/**
 * @file bitio.hpp
 * @brief Simple bit-level reader and writer.
 * @author Martin Pribylina (xpriby19)
 * @date 2026-05-06
 */

#pragma once

#include <cstdint>
#include <iostream>

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

    void flush() {
        if (bits_in_buf_ > 0) {
            buf_ = static_cast<uint8_t>(buf_ << (8 - bits_in_buf_));
            out_.put(static_cast<char>(buf_));
            buf_ = 0;
            bits_in_buf_ = 0;
        }
    }

private:
    std::ostream& out_;
    uint8_t       buf_;
    int           bits_in_buf_;
};

class BitReader {
public:
    /**
     * @param in  Input stream to read bytes from.
     */
    explicit BitReader(std::istream& in)
        : in_(in), buf_(0), bits_left_(0) {}

    bool read_bit() {
        if (bits_left_ == 0) {
            int c = in_.get();
            if (c == EOF) return false;
            buf_ = static_cast<uint8_t>(c);
            bits_left_ = 8;
        }
        --bits_left_;
        return (buf_ >> bits_left_) & 1u;
    }

    /**
     * Read n bits and return them as the least-significant bits of a uint32_t.
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
     * @return true if the underlying stream has reached EOF.
     */
    bool eof() const {
        return in_.peek() == EOF && bits_left_ == 0;
    }

private:
    std::istream& in_;
    uint8_t       buf_;
    int           bits_left_;
};
