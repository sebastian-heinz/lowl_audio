#include "lowl_audio_reader_mp3.h"

#include "audio/lowl_audio_format.h"

#define drmp3dec_decode_frame                     lowl_drmp3dec_decode_frame
#define drmp3_get_mp3_frame_count                 lowl_drmp3_get_mp3_frame_count
#define drmp3dec_init                             lowl_drmp3dec_init
#define drmp3_free                                lowl_drmp3_free
#define drmp3_init_memory                         lowl_drmp3_init_memory
#define drmp3_uninit                              lowl_drmp3_uninit
#define drmp3_init                                lowl_drmp3_init
#define drmp3dec_f32_to_s16                       lowl_drmp3dec_f32_to_s16
#define drmp3_bind_seek_table                     lowl_drmp3_bind_seek_table
#define drmp3_seek_to_pcm_frame                   lowl_drmp3_seek_to_pcm_frame
#define drmp3_open_memory_and_read_pcm_frames_s16 lowl_drmp3_open_memory_and_read_pcm_frames_s16
#define drmp3_calculate_seek_points               lowl_drmp3_calculate_seek_points
#define drmp3_init_memory_with_metadata           lowl_drmp3_init_memory_with_metadata
#define drmp3_get_mp3_and_pcm_frame_count         lowl_drmp3_get_mp3_and_pcm_frame_count
#define drmp3_open_and_read_pcm_frames_s16        lowl_drmp3_open_and_read_pcm_frames_s16
#define drmp3_read_pcm_frames_s16                 lowl_drmp3_read_pcm_frames_s16
#define drmp3_open_memory_and_read_pcm_frames_f32 lowl_drmp3_open_memory_and_read_pcm_frames_f32
#define drmp3_malloc                              lowl_drmp3_malloc
#define drmp3_version_string                      lowl_drmp3_version_string
#define drmp3_read_pcm_frames_f32                 lowl_drmp3_read_pcm_frames_f32
#define drmp3_get_pcm_frame_count                 lowl_drmp3_get_pcm_frame_count
#define drmp3_open_and_read_pcm_frames_f32        lowl_drmp3_open_and_read_pcm_frames_f32
#define drmp3_version                             lowl_drmp3_version

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>

#define ENCODED_BUFFER_DECODING_STEP (16384)
#define DECODED_BUFFER_SIZE (ENCODED_BUFFER_DECODING_STEP*32*8)

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderMp3::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {
    drmp3 mp3;
    if (!drmp3_init_memory(&mp3, p_buffer.get(), p_size, nullptr)) {
        error.set_error(ErrorCode::ReaderNoAudioData);
        return nullptr;
    }

    const AudioChannel channel = get_channel(mp3.channels);
    const SampleRate sample_rate = mp3.sampleRate;
    const size_t channel_count = get_channel_num(channel);
    const drmp3_uint64 total_frames = drmp3_get_pcm_frame_count(&mp3);

    std::unique_ptr<Sample[]> storage;
    if (total_frames > 0 && channel_count > 0) {
        storage = std::make_unique<Sample[]>(static_cast<size_t>(total_frames) * channel_count);
    }

    size_t decoded_frame_count = static_cast<size_t>(total_frames);
    if (storage != nullptr) {
        std::vector<float> pcm_frames(DECODED_BUFFER_SIZE, 0.0f);
        size_t frames_written = 0;
        while (frames_written < static_cast<size_t>(total_frames)) {
            const drmp3_uint64 frames_to_read = std::min<drmp3_uint64>(
                static_cast<drmp3_uint64>(DECODED_BUFFER_SIZE / channel_count),
                total_frames - frames_written
            );
            const drmp3_uint64 frames_read = drmp3_read_pcm_frames_f32(&mp3, frames_to_read, pcm_frames.data());
            if (frames_read == 0) {
                break;
            }
            for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
                Sample *dst = storage.get() + channel_index * static_cast<size_t>(total_frames) + frames_written;
                for (size_t frame_index = 0; frame_index < static_cast<size_t>(frames_read); frame_index++) {
                    dst[frame_index] = pcm_frames[frame_index * channel_count + channel_index];
                }
            }
            frames_written += static_cast<size_t>(frames_read);
        }
        decoded_frame_count = frames_written;

        if (frames_written < static_cast<size_t>(total_frames)) {
            std::unique_ptr<Sample[]> trimmed_storage;
            if (frames_written > 0) {
                trimmed_storage = std::make_unique<Sample[]>(frames_written * channel_count);
                for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
                    std::copy_n(
                        storage.get() + channel_index * static_cast<size_t>(total_frames),
                        frames_written,
                        trimmed_storage.get() + channel_index * frames_written
                    );
                }
            }
            storage = std::move(trimmed_storage);
        }
    }

    drmp3_uninit(&mp3);
    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(
        std::move(storage),
        decoded_frame_count,
        sample_rate,
        channel
    );
    return audio_data;
}

bool Lowl::Audio::AudioReaderMp3::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::MP3;
}
