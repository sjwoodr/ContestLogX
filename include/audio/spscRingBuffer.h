/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 *
 * Single-producer single-consumer lock-free ring buffer for int16 audio
 * samples. Used to decouple the audio-capture callback on the main thread
 * from the decoder worker thread (SPEC-005 research R6).
 */

#ifndef AUDIO_SPSCRINGBUFFER_H
#define AUDIO_SPSCRINGBUFFER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace clx::audio {

// Power-of-two-sized ring buffer of int16_t samples. Not resized after ctor.
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacity)
        : m_buf(capacity, 0), m_capacity(capacity)
    {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
    }

    size_t capacity() const { return m_capacity; }

    size_t available() const
    {
        const size_t h = m_head.load(std::memory_order_acquire);
        const size_t t = m_tail.load(std::memory_order_acquire);
        return (h >= t) ? (h - t) : (m_capacity - (t - h));
    }

    size_t freeSpace() const { return m_capacity - available() - 1; }

    // Producer side. Returns number of samples actually written (<= count).
    size_t push(const int16_t* data, size_t count)
    {
        const size_t h = m_head.load(std::memory_order_relaxed);
        const size_t t = m_tail.load(std::memory_order_acquire);
        const size_t free = (t > h) ? (t - h - 1) : (m_capacity - (h - t) - 1);
        const size_t toWrite = (count < free) ? count : free;
        if (toWrite == 0) return 0;

        const size_t first = (toWrite < m_capacity - h) ? toWrite : (m_capacity - h);
        std::memcpy(m_buf.data() + h, data, first * sizeof(int16_t));
        if (toWrite > first) {
            std::memcpy(m_buf.data(), data + first, (toWrite - first) * sizeof(int16_t));
        }
        m_head.store((h + toWrite) % m_capacity, std::memory_order_release);
        return toWrite;
    }

    // Consumer side. Returns number of samples actually read (<= count).
    size_t pop(int16_t* out, size_t count)
    {
        const size_t t = m_tail.load(std::memory_order_relaxed);
        const size_t h = m_head.load(std::memory_order_acquire);
        const size_t avail = (h >= t) ? (h - t) : (m_capacity - (t - h));
        const size_t toRead = (count < avail) ? count : avail;
        if (toRead == 0) return 0;

        const size_t first = (toRead < m_capacity - t) ? toRead : (m_capacity - t);
        std::memcpy(out, m_buf.data() + t, first * sizeof(int16_t));
        if (toRead > first) {
            std::memcpy(out + first, m_buf.data(), (toRead - first) * sizeof(int16_t));
        }
        m_tail.store((t + toRead) % m_capacity, std::memory_order_release);
        return toRead;
    }

private:
    std::vector<int16_t> m_buf;
    size_t m_capacity;
    std::atomic<size_t> m_head;  // producer writes here
    std::atomic<size_t> m_tail;  // consumer reads here
};

} // namespace clx::audio

#endif // AUDIO_SPSCRINGBUFFER_H
