#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include "audio/lowl_audio_frame.h"
#include "audio/lowl_audio_channel.h"
#include "audio/source/lowl_audio_source.h"

#include <readerwriterqueue.h>

#include <vector>

namespace Lowl::Audio {
    class AudioStream : public AudioSource {
    private:
        static constexpr size_t DEFAULT_STREAM_SIZE = 375000; // ~ 7 Seconds(3 MB) of stereo 32-bit float audio
        std::unique_ptr<moodycamel::ReaderWriterQueue<AudioFrame> > frame_queue;

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        ReadResult read(AudioFrame &audio_frame) override;

        bool write(const AudioFrame &p_audio_frame);

        size_l write(const std::vector<AudioFrame> &p_audio_frames);

        AudioStream(SampleRate p_sample_rate, AudioChannel p_channel, size_t size = DEFAULT_STREAM_SIZE);

        ~AudioStream() override = default;
    };
}

#endif
