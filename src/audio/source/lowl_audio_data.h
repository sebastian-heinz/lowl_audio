#ifndef LOWL_AUDIO_DATA_H
#define LOWL_AUDIO_DATA_H

#include "lowl_typedef.h"

#include "audio/lowl_audio_channel.h"

#include <memory>
#include <string>
#include <vector>

namespace Lowl::Audio {
    /**
     * Stores decoded clip data in planar sample buffers that voices can read from.
     */
    class AudioData {
    private:
        std::unique_ptr<Sample[]> storage;
        std::vector<Sample *> channel_ptrs;
        SampleRate sample_rate;
        AudioChannel channel;
        size_t frame_count = 0;
        std::string name;

        void rebuild_channel_ptrs();

    public:
        AudioData(const AudioData &) = delete;
        AudioData &operator=(const AudioData &) = delete;
        AudioData(AudioData &&) = delete;
        AudioData &operator=(AudioData &&) = delete;

        std::unique_ptr<AudioData> create_slice(double p_begin_sec, double p_end_sec);
        const Sample *get_channel_data(uint8_t p_channel) const;

        AudioData(
            std::unique_ptr<Sample[]> p_storage,
            size_t p_frame_count,
            SampleRate p_sample_rate,
            AudioChannel p_channel
        );
        ~AudioData();

        SampleRate get_sample_rate() const;
        AudioChannel get_channel() const;
        size_t get_channel_num() const;
        size_l get_frames_remaining() const;
        size_l get_frame_position() const;
        size_l get_frame_count() const;
        std::string get_name() const;
        void set_name(const std::string &p_name);
    };
}

#endif
