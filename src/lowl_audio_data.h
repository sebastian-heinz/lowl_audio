#ifndef LOWL_AUDIO_DATA_H
#define LOWL_AUDIO_DATA_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_channel.h"
#include "lowl_audio_source.h"

#include <vector>
#include <memory>
#include <atomic>


namespace Lowl {

    /**
     * represents a collection of frames that can be pushed into a stream repeatedly.
     * ex. sound effects, or any sound that should not be drained like a stream.
     */
    class AudioData : public AudioSource {

    private:
        std::vector<AudioFrame> frames;
        std::atomic<size_t> position;
        size_t size;

    public:
        /**
         * returns all frames.
         */
        std::vector<AudioFrame> get_frames();

        /**
         * returns a new AudioData created from a slice of its frames.
         */
        std::unique_ptr<AudioData> create_slice(double begin_sec, double end_sec);

        /**
         * reads a frame
         *
         * if the end is reached:
         *  - false will be returned indicating that the read frame is invalid.
         *  - position will be reset to the beginning, next call to read() will return the first frame again.
         */
        virtual bool read(AudioFrame &audio_frame) override;

        virtual size_l get_frames_remaining() const override;

        AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate, Channel p_channel);

        virtual ~AudioData();
    };
}


#endif
