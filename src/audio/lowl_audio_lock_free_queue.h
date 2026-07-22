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
        static_assert(std::atomic<size_t>::is_always_lock_free,
                      "BoundedSpscQueue counters must be lock-free for real-time audio safety");

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

} // namespace Lowl::Audio

#endif // LOWL_AUDIO_LOCK_FREE_QUEUE_H
