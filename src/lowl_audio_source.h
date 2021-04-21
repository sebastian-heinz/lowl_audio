#ifndef LOWL_AUDIO_SOURCE_H
#define LOWL_AUDIO_SOURCE_H

#include "lowl_typedef.h"
#include "lowl_channel.h"
#include "lowl_sample_format.h"
#include "lowl_audio_frame.h"

namespace Lowl {

    class AudioSource {

    protected:
        SampleRate sample_rate;
        Channel channel;
        // pan / gain ?

    public:
        AudioSource(SampleRate p_sample_rate, Channel p_channel);

        virtual ~AudioSource() = default;

        virtual bool read(AudioFrame &audio_frame) = 0;

        virtual size_l get_frames_remaining() const = 0;

        SampleRate get_sample_rate() const;

        Channel get_channel() const;

        int get_channel_num() const;

        SampleFormat get_sample_format() const;
    };
}


#endif //LOWL_AUDIO_SOURCE_H
