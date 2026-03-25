#include "lowl_audio_utilities.h"

#include <vector>

#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_stream.h"
#include "lowl_error.h"

std::unique_ptr<Lowl::Audio::AudioStream>
Lowl::Audio::Utilities::to_stream(const std::shared_ptr<AudioData> &p_audio_data, Error &error) {
    const size_t frame_count = p_audio_data->get_frame_count();
    std::vector<const Sample *> channels;
    channels.reserve(p_audio_data->get_channel_num());
    for (size_t channel_index = 0; channel_index < p_audio_data->get_channel_num(); channel_index++) {
        channels.push_back(p_audio_data->get_channel_data(static_cast<uint8_t>(channel_index)));
    }
    std::unique_ptr<AudioStream> stream =
        std::make_unique<AudioStream>(p_audio_data->get_sample_rate(), p_audio_data->get_channel(), frame_count);
    size_l written = stream->write_planar(channels, frame_count);
    if (written != frame_count) {
        error.set_error(ErrorCode::StreamWriteFailed);
        return nullptr;
    }
    return stream;
}
