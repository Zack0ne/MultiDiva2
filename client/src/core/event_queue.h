#pragma once
#include <atomic>
#include <cstddef>

namespace md {

// Bounded lock-free MPMC queue (Dmitry Vyukov's design). Push never blocks:
// when full it returns false and the item is dropped, so game-thread hooks can't stall.
template <typename T, size_t N>
class BoundedQueue {
    static_assert(N >= 2 && (N & (N - 1)) == 0, "N must be a power of two");

public:
    BoundedQueue()
    {
        for (size_t i = 0; i < N; i++)
            m_cells[i].seq.store(i, std::memory_order_relaxed);
    }

    bool TryPush(const T& value)
    {
        size_t pos = m_enqueue.load(std::memory_order_relaxed);
        for (;;) {
            Cell& cell = m_cells[pos & (N - 1)];
            size_t seq = cell.seq.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)pos;
            if (diff == 0) {
                if (m_enqueue.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell.value = value;
                    cell.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_enqueue.load(std::memory_order_relaxed);
            }
        }
    }

    bool TryPop(T& out)
    {
        size_t pos = m_dequeue.load(std::memory_order_relaxed);
        for (;;) {
            Cell& cell = m_cells[pos & (N - 1)];
            size_t seq = cell.seq.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
            if (diff == 0) {
                if (m_dequeue.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    out = cell.value;
                    cell.seq.store(pos + N, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_dequeue.load(std::memory_order_relaxed);
            }
        }
    }

private:
    struct Cell {
        std::atomic<size_t> seq;
        T value;
    };

    alignas(64) Cell m_cells[N];
    alignas(64) std::atomic<size_t> m_enqueue{0};
    alignas(64) std::atomic<size_t> m_dequeue{0};
};

} // namespace md
