#include "lowl_audio_reader_mp3.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "audio/reader/lowl_audio_reader_dr_lib.h"

#define ENCODED_BUFFER_DECODING_STEP (16384)
#define DECODED_BUFFER_SIZE (ENCODED_BUFFER_DECODING_STEP * 32 * 8)

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderMp3::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {
    DrLib::Mp3Decoder mp3(p_buffer.get(), p_size);
    if (!mp3.is_open()) {
        error.set_error(ErrorCode::ReaderNoAudioData);
        return nullptr;
    }

    const DrLib::Mp3Info mp3_info = mp3.get_info();
    const ChannelLayout layout = ChannelLayout::from_count(mp3_info.channels);
    if (!layout.is_valid()) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }
    const SampleRate sample_rate = mp3_info.sample_rate;
    const uint8_t channel_count = layout.channel_count;
    const uint64_t total_frames = mp3_info.total_pcm_frame_count;
    std::vector<float> pcm_frames(DECODED_BUFFER_SIZE, 0.0f);
    std::vector<float> decoded_samples;
    if (channel_count > 0) {
        const size_t max_reserved_frames = std::numeric_limits<size_t>::max() / channel_count;
        if (total_frames > 0 && total_frames <= static_cast<uint64_t>(max_reserved_frames)) {
            decoded_samples.reserve(static_cast<size_t>(total_frames) * channel_count);
        }
    }

    while (channel_count > 0) {
        const uint64_t frames_to_read = static_cast<uint64_t>(DECODED_BUFFER_SIZE / channel_count);
        const uint64_t frames_read = mp3.read_pcm_frames_f32(frames_to_read, pcm_frames.data());
        if (frames_read == 0) {
            break;
        }
        const size_t samples_read = static_cast<size_t>(frames_read) * channel_count;
        decoded_samples.insert(decoded_samples.end(), pcm_frames.data(), pcm_frames.data() + samples_read);
    }

    return create_audio_data(layout, decoded_samples, sample_rate, {}, error);
}

bool Lowl::Audio::AudioReaderMp3::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::MP3;
}
