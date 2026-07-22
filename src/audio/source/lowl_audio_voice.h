#ifndef LOWL_AUDIO_VOICE_H
#define LOWL_AUDIO_VOICE_H

#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>

#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_source.h"

namespace Lowl::Audio {
    /**
     * Playback instance for an `AudioData` clip with its own position, gain, and panning state.
     */
    class AudioVoice : public AudioSource {
        static_assert(std::atomic<size_t>::is_always_lock_free,
                      "AudioVoice positions must be lock-free for real-time audio safety");

    public:
        enum class PlaybackState : uint8_t {
            Stopped = 0,
            Playing = 1,
            Paused = 2,
        };

        /** Coherent intrinsic voice state; mixer attachment belongs to the owning composition layer. */
        struct PlaybackSnapshot {
            size_l frame_position = 0;
            PlaybackState playback_state = PlaybackState::Stopped;
        };

    private:
        static constexpr size_t NoPendingSeek = std::numeric_limits<size_t>::max();

        class PublishedState final {
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
            static_assert(PositionMask >= std::numeric_limits<size_t>::max() / sizeof(Sample),
                          "Published positions must cover every frame count that can fit in one sample allocation");

            static Storage pack(const PlaybackSnapshot &p_snapshot) {
                const Storage packed_position = static_cast<Storage>(p_snapshot.frame_position) & PositionMask;
                const Storage packed_playback_state =
                    (static_cast<Storage>(p_snapshot.playback_state) & PlaybackStateMask) << PlaybackStateShift;
                return packed_position | packed_playback_state;
            }

            static PlaybackSnapshot unpack(Storage p_storage) {
                PlaybackSnapshot snapshot{};
                snapshot.frame_position = static_cast<size_l>(p_storage & PositionMask);
                snapshot.playback_state =
                    static_cast<PlaybackState>((p_storage >> PlaybackStateShift) & PlaybackStateMask);
                return snapshot;
            }

        public:
            PublishedState() = default;

            PlaybackSnapshot load() const {
                return unpack(storage.load(std::memory_order_acquire));
            }

            void store(const PlaybackSnapshot &p_snapshot) {
                storage.store(pack(p_snapshot), std::memory_order_release);
            }

            template <typename UpdateFn>
            void update(UpdateFn &&p_update) {
                Storage current = storage.load(std::memory_order_acquire);
                while (true) {
                    PlaybackSnapshot snapshot = unpack(current);
                    p_update(snapshot);
                    const Storage updated = pack(snapshot);
                    if (storage.compare_exchange_weak(
                            current,
                            updated,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        return;
                    }
                }
            }

            template <typename UpdateFn>
            void try_update(UpdateFn &&p_update) {
                Storage current = storage.load(std::memory_order_acquire);
                PlaybackSnapshot snapshot = unpack(current);
                p_update(snapshot);
                const Storage updated = pack(snapshot);
                storage.compare_exchange_strong(
                    current,
                    updated,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire);
            }
        };

        std::shared_ptr<const AudioData> audio_data;
        mutable std::mutex control_state_mutex;
        std::atomic<size_t> render_position{};
        std::atomic<size_t> pending_seek_position{NoPendingSeek};
        PublishedState published_state{};
        std::atomic<uint64_t> control_state_serial{0};

        void begin_control_state_transition();
        void end_control_state_transition();

    public:
        explicit AudioVoice(std::shared_ptr<const AudioData> p_audio_data);

        RenderResult mix_into(AudioBlockView p_block,
                              const MixGainVector &p_upstream_gain) override;
        size_l get_frames_remaining() const override;
        size_l get_frame_position() const override;
        size_l get_frame_count() const override;

        void reset();
        void seek_time(TimeSeconds p_seconds);
        void seek_frame(size_t p_frame);
        /** Use this snapshot when a decision depends on more than one published playback field. */
        PlaybackSnapshot get_playback_snapshot() const;
        PlaybackState get_playback_state() const;
        void restart_playback();
        void pause_playback();
        void resume_playback();
        void stop_playback();

    };
} // namespace Lowl::Audio

#endif
