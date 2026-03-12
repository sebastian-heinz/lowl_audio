#include "lowl_audio_utilities.h"

#include "lowl_error.h"

std::unique_ptr<Lowl::Audio::AudioStream> Lowl::Audio::Utilities::to_stream(
    const std::shared_ptr<AudioData> &p_audio_data, Error &error
) {
    std::vector<AudioFrame> audio_frames = p_audio_data->get_frames();
    std::unique_ptr<AudioStream> stream = std::make_unique<AudioStream>(
        p_audio_data->get_sample_rate(), p_audio_data->get_channel(), audio_frames.size()
    );
    size_l written = stream->write(audio_frames);
    if (written != audio_frames.size()) {
        error.set_error(ErrorCode::StreamWriteFailed);
        return nullptr;
    }
    return stream;
}
