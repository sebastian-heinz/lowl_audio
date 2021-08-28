#ifndef LOWL_AUDIO_SOURCE_H
#define LOWL_AUDIO_SOURCE_H

#include "lowl_typedef.h"
#include "lowl_channel.h"
#include "lowl_sample_format.h"
#include "lowl_audio_frame.h"

#include <atomic>

namespace Lowl {

    class AudioSource {

    public:
        enum class ReadResult {
            Read = 0,
            Pause = 1,
            End = 2,
        };

    private:
        std::atomic<Volume> volume;
        std::atomic<Volume> panning;

    protected:
        SampleRate sample_rate;
        Channel channel;
        std::atomic<bool> is_playing;

        void process_volume(AudioFrame &audio_frame);

        void process_panning(AudioFrame &audio_frame);

    public:
        AudioSource(SampleRate p_sample_rate, Channel p_channel);

        virtual ~AudioSource() = default;

        virtual ReadResult read(AudioFrame &audio_frame) = 0;

        virtual size_l get_frames_remaining() const = 0;

        virtual size_l get_frame_position() const = 0;

        SampleRate get_sample_rate() const;

        Channel get_channel() const;

        int get_channel_num() const;

        SampleFormat get_sample_format() const;

        void set_volume(Volume p_volume);

        Volume get_volume();

        void set_panning(Panning p_panning);

        Panning get_panning();

        void pause();

        bool is_pause();

        void play();

        bool is_play();
    };
}


#endif //LOWL_AUDIO_SOURCE_H
