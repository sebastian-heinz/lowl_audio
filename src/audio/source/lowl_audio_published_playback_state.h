#ifndef LOWL_AUDIO_PUBLISHED_PLAYBACK_STATE_H
#define LOWL_AUDIO_PUBLISHED_PLAYBACK_STATE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Lowl::Audio::Detail {
    /**
     * Internal lock-free publication cell used by AudioVoice. Snapshot and state
     * remain template parameters so the storage primitive can be tested directly
     * without allocating an impractically large AudioData object.
     */
    template <typename Snapshot, typename PlaybackState, typename SampleType> class PublishedPlaybackState final {
    private:
        using Storage = uint64_t;

        // Two high bits encode the three playback states. The remaining 62 bits
        // cover every Sample frame index that can exist in one allocation.
        static constexpr Storage PlaybackStateShift = 62;
        static constexpr Storage PositionMask = (Storage{1} << PlaybackStateShift) - 1;
        static constexpr Storage PlaybackStateMask = 0x3ULL;

        std::atomic<Storage> storage{};

        static_assert(std::atomic<Storage>::is_always_lock_free,
                      "AudioVoice published state must be lock-free for real-time audio safety");
        static_assert(std::numeric_limits<size_t>::digits <= std::numeric_limits<Storage>::digits,
                      "AudioVoice frame positions must fit in published-state storage");
        static_assert(static_cast<Storage>(PlaybackState::Paused) <= PlaybackStateMask,
                      "Published playback state must fit in its reserved bits");
        static_assert(PositionMask >= std::numeric_limits<size_t>::max() / sizeof(SampleType),
                      "Published positions must cover every frame count that can fit in one sample allocation");

        static Storage pack(const Snapshot &p_snapshot) {
            const Storage packed_position = static_cast<Storage>(p_snapshot.frame_position) & PositionMask;
            const Storage packed_playback_state = (static_cast<Storage>(p_snapshot.playback_state) & PlaybackStateMask)
                                                  << PlaybackStateShift;
            return packed_position | packed_playback_state;
        }

        static Snapshot unpack(const Storage p_storage) {
            Snapshot snapshot{};
            snapshot.frame_position = static_cast<size_t>(p_storage & PositionMask);
            snapshot.playback_state = static_cast<PlaybackState>((p_storage >> PlaybackStateShift) & PlaybackStateMask);
            return snapshot;
        }

    public:
        PublishedPlaybackState() = default;

        Snapshot load() const {
            return unpack(storage.load(std::memory_order_acquire));
        }

        void store(const Snapshot &p_snapshot) {
            storage.store(pack(p_snapshot), std::memory_order_release);
        }

        template <typename UpdateFn> void update(UpdateFn &&p_update) {
            Storage current = storage.load(std::memory_order_acquire);
            while (true) {
                Snapshot snapshot = unpack(current);
                p_update(snapshot);
                const Storage updated = pack(snapshot);
                if (storage.compare_exchange_weak(
                        current, updated, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return;
                }
            }
        }

        template <typename UpdateFn> void try_update(UpdateFn &&p_update) {
            Storage current = storage.load(std::memory_order_acquire);
            Snapshot snapshot = unpack(current);
            p_update(snapshot);
            const Storage updated = pack(snapshot);
            storage.compare_exchange_strong(current, updated, std::memory_order_acq_rel, std::memory_order_acquire);
        }
    };
} // namespace Lowl::Audio::Detail

#endif // LOWL_AUDIO_PUBLISHED_PLAYBACK_STATE_H
