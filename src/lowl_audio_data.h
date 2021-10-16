#ifndef LOWL_AUDIO_DATA_H
#define LOWL_AUDIO_DATA_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_audio_channel.h"
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
        std::atomic<size_t> seek_position;
        size_t size;
        std::atomic_flag is_not_reset;

    public:
        /**
         * returns all frames.
         */
        std::vector<AudioFrame> get_frames();

        /**
         * returns a new AudioData created from a slice of its frames.
         */
        std::unique_ptr<AudioData> create_slice(double p_begin_sec, double p_end_sec);

        /**
         * resets the current position to the start
         */
        void reset();

        /**
         * sets the play head to the specified time
         */
        void seek_time(TimeSeconds p_seconds);

        /**
         * sets the play head to the specified frame
         */
        void seek_frame(size_t p_frame);

        /**
         * reads a frame
         *
         * if the end is reached:
         *  - false will be returned indicating that the read frame is invalid.
         *  - position will be reset to the beginning, next call to read() will return the first frame again.
         */
        virtual ReadResult read(AudioFrame &audio_frame) override;

        virtual size_l get_frames_remaining() const override;

        virtual size_l get_frame_position() const override;

        virtual size_l get_frame_count() const override;

        AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate, AudioChannel p_channel);

        virtual ~AudioData();
    };
}


#endif
