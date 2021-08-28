#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_channel.h"
#include "lowl_audio_source.h"

#include <readerwriterqueue.h>

#include <vector>
#include <memory>

namespace Lowl {

    class AudioStream : public AudioSource {

    private:
        std::unique_ptr<moodycamel::ReaderWriterQueue<AudioFrame>> frame_queue;

    public:
        virtual size_l get_frames_remaining() const override;

        virtual size_l get_frame_position() const override;

        virtual ReadResult read(AudioFrame &audio_frame) override;

        bool write(const AudioFrame &p_audio_frame);

        void write(const std::vector<AudioFrame> &p_audio_frames);

        AudioStream(SampleRate p_sample_rate, Channel p_channel);

        virtual ~AudioStream() = default;
    };
}

#endif