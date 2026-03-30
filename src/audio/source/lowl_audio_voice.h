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
    public:
        enum class PlaybackState : uint8_t {
            Stopped = 0,
            Playing = 1,
            Paused = 2,
        };

    private:
        static constexpr size_t NoPendingSeek = std::numeric_limits<size_t>::max();

        struct PublishedStateSnapshot {
            uint32_t position = 0;
            PlaybackState playback_state = PlaybackState::Stopped;
            bool detached = false;
        };

        class PublishedState final {
        private:
            using Storage = uint64_t;

            static constexpr Storage PositionMask = 0xFFFFFFFFULL;
            static constexpr Storage PlaybackStateShift = 32;
            static constexpr Storage PlaybackStateMask = 0x3ULL;
            static constexpr Storage DetachedShift = 34;
            static constexpr Storage DetachedMask = 0x1ULL;

            std::atomic<Storage> storage{};

            static Storage pack(PublishedStateSnapshot p_state) {
                const Storage packed_position = static_cast<Storage>(p_state.position) & PositionMask;
                const Storage packed_playback_state =
                    (static_cast<Storage>(p_state.playback_state) & PlaybackStateMask) << PlaybackStateShift;
                const Storage packed_detached =
                    (static_cast<Storage>(p_state.detached) & DetachedMask) << DetachedShift;
                return packed_position | packed_playback_state | packed_detached;
            }

            static PublishedStateSnapshot unpack(Storage p_storage) {
                PublishedStateSnapshot snapshot{};
                snapshot.position = static_cast<uint32_t>(p_storage & PositionMask);
                snapshot.playback_state =
                    static_cast<PlaybackState>((p_storage >> PlaybackStateShift) & PlaybackStateMask);
                snapshot.detached = ((p_storage >> DetachedShift) & DetachedMask) != 0;
                return snapshot;
            }

        public:
            PublishedState() = default;

            PublishedStateSnapshot load() const {
                return unpack(storage.load(std::memory_order_acquire));
            }

            void store(PublishedStateSnapshot p_state) {
                storage.store(pack(p_state), std::memory_order_release);
            }

            template <typename UpdateFn>
            void update(UpdateFn &&p_update) {
                Storage current = storage.load(std::memory_order_acquire);
                while (true) {
                    PublishedStateSnapshot snapshot = unpack(current);
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

        RenderResult render(AudioBlockView p_block) override;
        size_l get_frames_remaining() const override;
        size_l get_frame_position() const override;
        size_l get_frame_count() const override;

        void reset();
        void seek_time(TimeSeconds p_seconds);
        void seek_frame(size_t p_frame);
        bool is_detached() const;
        PlaybackState get_playback_state() const;
        void restart_playback();
        void pause_playback();
        void resume_playback();
        void stop_playback();

        void on_added_to_mixer() override;
        void on_removed_from_mixer() override;
    };
} // namespace Lowl::Audio

#endif
