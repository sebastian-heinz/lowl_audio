#ifndef LOWL_AUDIO_SOURCE_H
#define LOWL_AUDIO_SOURCE_H

#include <array>
#include <atomic>
#include <mutex>
#include <string>

#include "audio/lowl_audio_buffer.h"
#include "audio/lowl_audio_format.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Base class for anything that can render audio into a block for a mixer or device.
     * Implementations used on a real-time render thread must not allocate, block, or perform
     * unbounded work in mix_into(). AudioSource does not track topology or ownership; callers
     * using sources outside AudioGraph are responsible for lifetime and render consistency.
     */
    class AudioSource {
        static_assert(std::atomic<uint64_t>::is_always_lock_free,
                      "AudioSource gain generation must be lock-free for real-time audio safety");

    public:
        struct MixGainVector {
            std::array<Sample, AudioBlockView::MAX_CHANNELS> values{};

            MixGainVector() {
                values.fill(static_cast<Sample>(1));
            }

            LOWL_INLINE Sample &operator[](const uint8_t p_channel) {
                return values[static_cast<size_t>(p_channel)];
            }

            LOWL_INLINE const Sample &operator[](const uint8_t p_channel) const {
                return values[static_cast<size_t>(p_channel)];
            }
        };

        enum class RenderState {
            Ok = 0,
            Starved = 1,
            Finished = 2,
            Remove = 3,
            Error = 4,
        };

        struct RenderResult {
            uint32_t frames_produced = 0;
            RenderState state = RenderState::Ok;
        };

    private:
        struct GainCache {
            uint64_t generation = 0;
            std::array<Sample, AudioBlockView::MAX_CHANNELS> local_gains{};
        };

        std::atomic<Volume> volume{DEFAULT_VOLUME};
        std::atomic<Volume> panning{DEFAULT_PANNING};
        std::atomic<uint64_t> gain_generation{1};
        mutable std::mutex name_mutex;
        std::string name;
        mutable GainCache gain_cache{};
        const int left_channel_index;
        const int right_channel_index;

    protected:
        const AudioFormat audio_format;
        std::atomic<bool> playback_enabled{true};

        static void clear_block(AudioBlockView p_block);

        static void mix_scaled_channel(const Sample *LOWL_RESTRICT p_src,
                                       Sample *LOWL_RESTRICT p_dst,
                                       Sample p_gain,
                                       uint32_t p_frame_count);

        MixGainVector compose_gain_vector(const MixGainVector &p_upstream_gain) const;

        static MixGainVector make_unity_gain_vector();

    public:
        explicit AudioSource(AudioFormat p_audio_format);

        virtual ~AudioSource() = default;

        virtual RenderResult render(AudioBlockView p_block);

        virtual RenderResult mix_into(AudioBlockView p_block,
                                      const MixGainVector &p_upstream_gain) = 0;

        virtual size_l get_frames_remaining() const = 0;

        virtual size_l get_frame_position() const = 0;

        virtual size_l get_frame_count() const = 0;

        std::string get_name() const;

        void set_name(const std::string &p_name);

        const AudioFormat &get_audio_format() const;

        uint8_t get_channel_count() const;

        void set_volume(Volume p_volume);

        Volume get_volume() const;

        void set_panning(Panning p_panning);

        Panning get_panning() const;

        void pause();

        bool is_pause() const;

        void play();

        bool is_play() const;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_SOURCE_H
