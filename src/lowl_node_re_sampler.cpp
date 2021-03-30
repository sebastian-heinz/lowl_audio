#include "lowl_node_re_sampler.h"

Lowl::NodeReSampler::NodeReSampler(SampleRate p_sample_rate_src,
                                   SampleRate p_sample_rate_dst,
                                   Channel p_channel,
                                   size_t p_sample_buffer_size,
                                   double p_req_trans_band) {
    re_sampler = std::make_unique<ReSampler>(
            p_sample_rate_src,
            p_sample_rate_dst,
            p_channel,
            p_sample_buffer_size,
            p_req_trans_band
    );
}

bool Lowl::NodeReSampler::process(Lowl::AudioFrame &p_audio_frame) {
    re_sampler->write(p_audio_frame, 15);
    if (re_sampler->read(p_audio_frame)) {
        return true;
    }
    return false;
}


//void Lowl::NodeReSampler::set_output_sample_rate(Lowl::SampleRate p_output_sample_rate) {
//
//    if (p_output_sample_rate != sample_rate) {
//        output_sample_rate = p_output_sample_rate;
//        re_sampler = std::make_unique<ReSampler>(
//                sample_rate, output_sample_rate, channel, 512, 2.0
//        );
//        require_resampling = true;
//    } else {
//        require_resampling = false;
//    }
//    is_sample_rate_changing.clear(std::memory_order_release);
//}
//
//Lowl::SampleRate Lowl::NodeReSampler::get_output_sample_rate() const {
//    return output_sample_rate;
//}
