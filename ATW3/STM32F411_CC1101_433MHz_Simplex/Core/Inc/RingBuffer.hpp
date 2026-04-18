/*
 * RingBuffer.hpp
 *
 *  Created on: Apr 16, 2026
 *      Author: mgi
 */

#pragma once
#include <cstdint>

template <size_t Size>
class RingBuffer {
    uint8_t buffer[Size];
    volatile size_t head = 0;
    volatile size_t tail = 0;
public:
    bool push(uint8_t val) {
        size_t next = (head + 1) % Size;
        if (next == tail) return false;
        buffer[head] = val;
        head = next;
        return true;
    }
    bool pop(uint8_t &val) {
        if (head == tail) return false;
        val = buffer[tail];
        tail = (tail + 1) % Size;
        return true;
    }
    size_t count() const {
        return (head >= tail) ? (head - tail) : (Size - tail + head);
    }
};
