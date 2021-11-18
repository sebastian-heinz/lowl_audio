#include "lowl_audio_utilities.h"

std::unique_ptr<Lowl::ReleasePool> Lowl::Audio::Utilities::release_pool = std::make_unique<ReleasePool>();

std::unique_ptr<Lowl::Audio::AudioStream> Lowl::Audio::Utilities::to_stream(std::shared_ptr<AudioData> p_audio_data) {
    std::unique_ptr<AudioStream> stream = std::make_unique<AudioStream>(
            p_audio_data->get_sample_rate(), p_audio_data->get_channel()
    );
    stream->write(p_audio_data->get_frames());
    return stream;
}
