#ifndef LOWL_AUDIO_STREAM_H
#define LOWL_AUDIO_STREAM_H

#include "lowl_sample_format.h"
#include "lowl_error.h"
#include "lowl_audio_frame.h"
#include "lowl_channel.h"
#include "lowl_sample_rate.h"


#include <readerwriterqueue.h>
#include <CDSPResampler.h>

#include <vector>

namespace Lowl {

    class AudioStream {

    private:
        SampleRate sample_rate;
        SampleRate output_sample_rate;
        Channel channel;
        moodycamel::ReaderWriterQueue<AudioFrame> *frame_queue;
        moodycamel::ReaderWriterQueue<AudioFrame> *resample_queue;
        uint32_t frames_in;
        uint32_t frames_out;
        std::vector<AudioFrame> resamples;
        std::vector<std::vector<double>> samples;
        std::vector<std::unique_ptr<r8b::CDSPResampler24>> re_samplers;
        size_t re_sampler_sample_buffer_size;
        std::atomic_flag is_sample_rate_changing = ATOMIC_FLAG_INIT;
        bool require_resampling;


    public:
        void set_output_sample_rate(SampleRate p_output_sample_rate);

        uint32_t get_num_frame_write() const;

        uint32_t get_num_frame_read() const;

        SampleFormat get_sample_format() const;

        SampleRate get_sample_rate() const;

        SampleRate get_output_sample_rate() const;

        Channel get_channel() const;

        int get_channel_num() const;

        size_t get_num_frame_queued() const;

        bool read(AudioFrame &audio_frame);

        std::vector<AudioFrame> read_all();

        bool write(const AudioFrame &p_audio_frame);

        void write(const std::vector<AudioFrame> &p_audio_frames);

        AudioStream(SampleRate p_sample_rate, Channel p_channel, size_t p_re_sampler_sample_buffer_size);

        AudioStream(SampleRate p_sample_rate, Channel p_channel);

        ~AudioStream();

#ifdef LOWL_PROFILING
    public:
        uint64_t produce_count;
        double produce_total_duration;
        double produce_max_duration;
        double produce_min_duration;
        double produce_avg_duration;
#endif
    };
}


#endif