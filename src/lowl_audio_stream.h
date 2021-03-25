#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_channel.h"
#include "lowl_sample_rate.h"

#include <readerwriterqueue.h>

#include <vector>

namespace Lowl {

    class AudioStream {

    private:
        SampleRate sample_rate;
        Channel channel;
        moodycamel::ReaderWriterQueue<AudioFrame> *buffer;
        uint32_t frames_in;
        uint32_t frames_out;

    public:
        uint32_t get_num_frame_write() const;

        uint32_t get_num_frame_read() const;

        SampleFormat get_sample_format() const;

        SampleRate get_sample_rate() const;

        Channel get_channel() const;

        int get_channel_num() const;

        size_t get_num_frame_queued() const;

        bool read(AudioFrame &audio_frame);

        std::vector<AudioFrame> read();

        bool write(const AudioFrame &p_audio_frame);

        void write(const std::vector<AudioFrame> &p_audio_frames);

        void drain();

        AudioStream(SampleRate p_sample_rate, Channel p_channel);

        ~AudioStream();
    };
}


#endif