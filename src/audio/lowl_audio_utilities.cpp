#include "lowl_audio_utilities.h"

#include <algorithm>
#include <vector>

#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_stream.h"
#include "lowl_error.h"

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::Utilities::clone_audio_data(const std::shared_ptr<AudioData> &p_audio_data) {
    if (!p_audio_data) {
        return nullptr;
    }

    const size_t frame_count = p_audio_data->get_frame_count();
    const uint8_t channel_count = p_audio_data->get_channel_count();
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0 && channel_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
        for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
            const Sample *source = p_audio_data->get_channel_data(channel_index);
            if (source != nullptr) {
                std::copy_n(source, frame_count, storage.get() + static_cast<size_t>(channel_index) * frame_count);
            }
        }
    }

    std::unique_ptr<AudioData> clone = std::make_unique<AudioData>(
        std::move(storage), frame_count, p_audio_data->get_audio_format());
    clone->set_name(p_audio_data->get_name());
    return clone;
}

std::unique_ptr<Lowl::Audio::AudioStream>
Lowl::Audio::Utilities::to_stream(const std::shared_ptr<AudioData> &p_audio_data, Error &error) {
    const size_t frame_count = p_audio_data->get_frame_count();
    std::vector<const Sample *> channels;
    channels.reserve(p_audio_data->get_channel_count());
    for (uint8_t channel_index = 0; channel_index < p_audio_data->get_channel_count(); channel_index++) {
        channels.push_back(p_audio_data->get_channel_data(channel_index));
    }
    std::unique_ptr<AudioStream> stream = std::make_unique<AudioStream>(
        p_audio_data->get_audio_format(), frame_count);
    size_l written = stream->write_planar(channels, frame_count);
    if (written != frame_count) {
        error.set_error(ErrorCode::StreamWriteFailed);
        return nullptr;
    }
    return stream;
}
