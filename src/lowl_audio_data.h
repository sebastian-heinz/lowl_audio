#ifndef LOWL_AUDIO_DATA_H
#define LOWL_AUDIO_DATA_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_channel.h"
#include "lowl_sample_rate.h"
#include "lowl_audio_stream.h"

#include <vector>

namespace Lowl {

    /**
     * represents a collection of frames that can be pushed into a stream repeatedly.
     * ex. sound effects, or any sound that should not be drained like a stream.
     */
    class AudioData {

    private:
        SampleRate sample_rate;
        Channel channel;
        std::vector<AudioFrame> frames;
        size_t position;
        std::atomic_flag do_read = ATOMIC_FLAG_INIT;

    public:
        SampleFormat get_sample_format() const;

        SampleRate get_sample_rate() const;

        Channel get_channel() const;

        int get_channel_num() const;

        /**
         * signals read to interrupt
         * next read call will start reading data from beginning.
         */
        void cancel_read();

        /**
         * returns all frames.
         */
        std::vector<AudioFrame> get_frames();

        /**
         * returns all frames.
         */
        std::unique_ptr<AudioStream> to_stream();

        /**
         * reads a frame
         *
         * if the end is reached:
         *  - false will be returned indicating that the read frame is invalid.
         *  - position will be reset to the beginning, next call to read() will return the first frame again.
         */
        bool read(AudioFrame &audio_frame);

        AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate, Channel p_channel);

        ~AudioData();
    };
}


#endif