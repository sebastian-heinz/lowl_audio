#ifndef LOWL_RE_SAMPLER_R8B_H
#define LOWL_RE_SAMPLER_R8B_H

#include "audio/convert/lowl_audio_re_sampler.h"

#include "audio/lowl_audio_data.h"

#include <readerwriterqueue.h>
#include <CDSPResampler.h>

#include <vector>

namespace Lowl::Audio {
    class ReSamplerR8b  : public ReSampler {
    private:
        size_t num_channel;
        size_t current_frame;
        std::vector<AudioFrame> resamples;
        std::vector<std::vector<double>> samples;
        std::vector<std::unique_ptr<r8b::CDSPResampler24>> re_samplers;
        size_t sample_buffer_size;
        size_t total_frames_in;
        size_t total_re_sampled_frames;
        moodycamel::ReaderWriterQueue<AudioFrame> *resample_queue;

    public:
            ReSamplerR8b(SampleRate p_sample_rate_src,
                  SampleRate p_sample_rate_dst,
                  AudioChannel p_channel,
                  size_t p_sample_buffer_size,
                  double p_req_trans_band
        );


        static std::unique_ptr<Lowl::Audio::AudioData>
        resample(std::shared_ptr<AudioData> p_audio_data, SampleRate p_sample_rate_dst);

        bool read(AudioFrame &audio_frame) override;
        void write(const AudioFrame &p_audio_frame) override;
        void flush() override;

        ~ReSamplerR8b() override;
    };
}

#endif
