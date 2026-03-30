#ifndef LOWL_AUDIO_LOCK_FREE_QUEUE_H
#define LOWL_AUDIO_LOCK_FREE_QUEUE_H

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace Lowl::Audio {
    template <typename T, size_t Capacity>
    class BoundedSpscQueue {
    private:
        static_assert(Capacity >= 2, "BoundedSpscQueue capacity must be at least 2");
        static_assert((Capacity & (Capacity - 1)) == 0, "BoundedSpscQueue capacity must be a power of 2");

        static constexpr size_t Mask = Capacity - 1;

        alignas(64) std::atomic<size_t> head{0};
        alignas(64) std::atomic<size_t> tail{0};
        std::array<T, Capacity> storage{};

    public:
        BoundedSpscQueue() = default;
        BoundedSpscQueue(const BoundedSpscQueue &) = delete;
        BoundedSpscQueue &operator=(const BoundedSpscQueue &) = delete;

        template <typename U>
        bool try_enqueue(U &&p_item) {
            const size_t current_tail = tail.load(std::memory_order_relaxed);
            const size_t current_head = head.load(std::memory_order_acquire);
            if (current_tail - current_head >= Capacity) {
                return false;
            }

            storage[current_tail & Mask] = std::forward<U>(p_item);
            tail.store(current_tail + 1, std::memory_order_release);
            return true;
        }

        bool try_dequeue(T &p_item) {
            const size_t current_head = head.load(std::memory_order_relaxed);
            const size_t current_tail = tail.load(std::memory_order_acquire);
            if (current_head == current_tail) {
                return false;
            }

            p_item = std::move(storage[current_head & Mask]);
            head.store(current_head + 1, std::memory_order_release);
            return true;
        }
    };

    template <typename T, size_t Capacity>
    class BoundedMpscQueue {
    private:
        static_assert(Capacity >= 2, "BoundedMpscQueue capacity must be at least 2");
        static_assert((Capacity & (Capacity - 1)) == 0, "BoundedMpscQueue capacity must be a power of 2");

        struct Cell {
            std::atomic<size_t> sequence{0};
            T data{};
        };

        static constexpr size_t Mask = Capacity - 1;

        alignas(64) std::atomic<size_t> enqueue_pos{0};
        alignas(64) std::atomic<size_t> dequeue_pos{0};
        std::array<Cell, Capacity> storage{};

    public:
        BoundedMpscQueue() {
            for (size_t index = 0; index < Capacity; index++) {
                storage[index].sequence.store(index, std::memory_order_relaxed);
            }
        }

        BoundedMpscQueue(const BoundedMpscQueue &) = delete;
        BoundedMpscQueue &operator=(const BoundedMpscQueue &) = delete;

        template <typename U>
        bool try_enqueue(U &&p_item) {
            Cell *cell = nullptr;
            size_t position = enqueue_pos.load(std::memory_order_relaxed);

            while (true) {
                cell = &storage[position & Mask];
                const size_t sequence = cell->sequence.load(std::memory_order_acquire);
                const intptr_t diff =
                    static_cast<intptr_t>(sequence) - static_cast<intptr_t>(position);

                if (diff == 0) {
                    if (enqueue_pos.compare_exchange_weak(
                            position,
                            position + 1,
                            std::memory_order_relaxed,
                            std::memory_order_relaxed)) {
                        break;
                    }
                } else if (diff < 0) {
                    return false;
                } else {
                    position = enqueue_pos.load(std::memory_order_relaxed);
                }
            }

            cell->data = std::forward<U>(p_item);
            cell->sequence.store(position + 1, std::memory_order_release);
            return true;
        }

        bool try_dequeue(T &p_item) {
            size_t position = dequeue_pos.load(std::memory_order_relaxed);

            while (true) {
                Cell &cell = storage[position & Mask];
                const size_t sequence = cell.sequence.load(std::memory_order_acquire);
                const intptr_t diff =
                    static_cast<intptr_t>(sequence) - static_cast<intptr_t>(position + 1);

                if (diff == 0) {
                    dequeue_pos.store(position + 1, std::memory_order_relaxed);
                    p_item = std::move(cell.data);
                    cell.sequence.store(position + Capacity, std::memory_order_release);
                    return true;
                }
                if (diff < 0) {
                    return false;
                }

                position = dequeue_pos.load(std::memory_order_relaxed);
            }
        }
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_LOCK_FREE_QUEUE_H
