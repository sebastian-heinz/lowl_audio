#include "lowl_audio_reader_mp3.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "audio/lowl_audio_format.h"

#define drmp3dec_decode_frame lowl_drmp3dec_decode_frame
#define drmp3_get_mp3_frame_count lowl_drmp3_get_mp3_frame_count
#define drmp3dec_init lowl_drmp3dec_init
#define drmp3_free lowl_drmp3_free
#define drmp3_init_memory lowl_drmp3_init_memory
#define drmp3_uninit lowl_drmp3_uninit
#define drmp3_init lowl_drmp3_init
#define drmp3dec_f32_to_s16 lowl_drmp3dec_f32_to_s16
#define drmp3_bind_seek_table lowl_drmp3_bind_seek_table
#define drmp3_seek_to_pcm_frame lowl_drmp3_seek_to_pcm_frame
#define drmp3_open_memory_and_read_pcm_frames_s16 lowl_drmp3_open_memory_and_read_pcm_frames_s16
#define drmp3_calculate_seek_points lowl_drmp3_calculate_seek_points
#define drmp3_init_memory_with_metadata lowl_drmp3_init_memory_with_metadata
#define drmp3_get_mp3_and_pcm_frame_count lowl_drmp3_get_mp3_and_pcm_frame_count
#define drmp3_open_and_read_pcm_frames_s16 lowl_drmp3_open_and_read_pcm_frames_s16
#define drmp3_read_pcm_frames_s16 lowl_drmp3_read_pcm_frames_s16
#define drmp3_open_memory_and_read_pcm_frames_f32 lowl_drmp3_open_memory_and_read_pcm_frames_f32
#define drmp3_malloc lowl_drmp3_malloc
#define drmp3_version_string lowl_drmp3_version_string
#define drmp3_read_pcm_frames_f32 lowl_drmp3_read_pcm_frames_f32
#define drmp3_get_pcm_frame_count lowl_drmp3_get_pcm_frame_count
#define drmp3_open_and_read_pcm_frames_f32 lowl_drmp3_open_and_read_pcm_frames_f32
#define drmp3_version lowl_drmp3_version

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>

#define ENCODED_BUFFER_DECODING_STEP (16384)
#define DECODED_BUFFER_SIZE (ENCODED_BUFFER_DECODING_STEP * 32 * 8)

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderMp3::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {
    drmp3 mp3;
    if (!drmp3_init_memory(&mp3, p_buffer.get(), p_size, nullptr)) {
        error.set_error(ErrorCode::ReaderNoAudioData);
        return nullptr;
    }

    const ChannelLayout layout = ChannelLayout::from_count(static_cast<uint8_t>(mp3.channels));
    if (!layout.is_valid()) {
        drmp3_uninit(&mp3);
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }
    const SampleRate sample_rate = mp3.sampleRate;
    const uint8_t channel_count = layout.channel_count;
    const drmp3_uint64 total_frames = drmp3_get_pcm_frame_count(&mp3);
    std::vector<float> pcm_frames(DECODED_BUFFER_SIZE, 0.0f);
    std::vector<float> decoded_samples;
    if (channel_count > 0) {
        const size_t max_reserved_frames = std::numeric_limits<size_t>::max() / channel_count;
        if (total_frames > 0 && total_frames <= static_cast<drmp3_uint64>(max_reserved_frames)) {
            decoded_samples.reserve(static_cast<size_t>(total_frames) * channel_count);
        }
    }

    while (channel_count > 0) {
        const drmp3_uint64 frames_to_read = static_cast<drmp3_uint64>(DECODED_BUFFER_SIZE / channel_count);
        const drmp3_uint64 frames_read = drmp3_read_pcm_frames_f32(&mp3, frames_to_read, pcm_frames.data());
        if (frames_read == 0) {
            break;
        }
        const size_t samples_read = static_cast<size_t>(frames_read) * channel_count;
        decoded_samples.insert(decoded_samples.end(), pcm_frames.data(), pcm_frames.data() + samples_read);
    }

    drmp3_uninit(&mp3);
    return create_audio_data(layout, decoded_samples, sample_rate, {}, error);
}

bool Lowl::Audio::AudioReaderMp3::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::MP3;
}
