#ifndef LOWL_AUDIO_DATA_H
#define LOWL_AUDIO_DATA_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_channel.h"
#include "lowl_sample_rate.h"

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

    public:
        SampleFormat get_sample_format() const;

        SampleRate get_sample_rate() const;

        Channel get_channel() const;

        int get_channel_num() const;

        /**
         * returns all frames.
         */
        std::vector<AudioFrame> get_frames();

        /**
         * reads a frame
         *
         * if the end is reached:
         *  - false will be returned indicating that the read frame is invalid.
         *  - position will be reset to the beginning, next call to read() will return the first frame again.
         */
        bool read(AudioFrame &audio_frame);

        bool write(const AudioFrame &p_audio_frame);

        bool write(const std::vector<AudioFrame> &p_audio_frames);

        AudioData(SampleRate p_sample_rate, Channel p_channel);

        ~AudioData();
    };
}


#endif