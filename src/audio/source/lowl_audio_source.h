#ifndef LOWL_AUDIO_SOURCE_H
#define LOWL_AUDIO_SOURCE_H

#include <atomic>
#include <string>

#include "audio/backend/lowl_audio_device_properties.h"
#include "audio/lowl_audio_buffer.h"
#include "audio/lowl_audio_channel.h"
#include "audio/lowl_audio_sample_format.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Base class for anything that can render audio into a block for a mixer or device.
     */
    class AudioSource {
    public:
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
        std::atomic<Volume> volume{};
        std::atomic<Volume> panning{};
        std::string name;

    protected:
        SampleRate sample_rate;
        AudioChannel channel;
        std::atomic<bool> playback_enabled{true};

        void process_volume(AudioBlockView p_block) const;

        void process_panning(AudioBlockView p_block) const;

    public:
        AudioSource(SampleRate p_sample_rate, AudioChannel p_channel);

        virtual ~AudioSource() = default;

        virtual RenderResult render(AudioBlockView p_block) = 0;

        virtual void on_added_to_mixer();

        virtual void on_removed_from_mixer();

        virtual size_l get_frames_remaining() const = 0;

        virtual size_l get_frame_position() const = 0;

        virtual size_l get_frame_count() const = 0;

        std::string get_name() const;

        void set_name(const std::string &p_name);

        SampleRate get_sample_rate() const;

        AudioChannel get_channel() const;

        size_t get_channel_num() const;

        SampleFormat get_sample_format() const;

        AudioDeviceProperties get_properties() const;

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
