#include "lowl_audio_reader_opus.h"

#include <opusfile.h>

#include "audio/lowl_audio_format.h"
#include "audio/lowl_audio_utilities.h"

#define OPUS_SAMPLE_RATE (48000)
#define OPUS_BUFFER_SIZE_MS (500)

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderOpus::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    int _error = 0;
    std::unique_ptr<OggOpusFile, decltype(&op_free)> ogg_file(op_open_memory(p_buffer.get(), p_size, &_error), op_free);

    if (ogg_file == nullptr) {
        return nullptr;
    }

    SampleRate sample_rate = OPUS_SAMPLE_RATE;
    uint32_t channel_count = static_cast<uint32_t>(op_channel_count(ogg_file.get(), -1));
    AudioChannel channel = get_channel(channel_count);
    ogg_int64_t sample_count = op_pcm_total(ogg_file.get(), -1);
    const size_t frame_count = sample_count > 0 ? static_cast<size_t>(sample_count) : 0;
    size_t buffer_size = Lowl::Audio::ms_to_samples(OPUS_BUFFER_SIZE_MS, sample_rate, channel);
    std::vector<float> buffer(buffer_size, 0.0f);
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0 && channel_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * channel_count);
    }
    size_t frames_read_total = 0;

    while (frames_read_total < frame_count) {
        int samples_read_per_channel = op_read_float(ogg_file.get(), buffer.data(), (int)buffer.size(), nullptr);
        if (samples_read_per_channel < 0) {
            error.set_error(ErrorCode::OpusFileCanNotParseOpusFile);
            return nullptr;
        }
        if (samples_read_per_channel == 0) {
            break;
        }
        const size_t frames_read = static_cast<size_t>(samples_read_per_channel);
        for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
            Sample *dst = storage ? storage.get() + channel_index * frame_count + frames_read_total : nullptr;
            if (dst != nullptr) {
                for (size_t frame_index = 0; frame_index < frames_read; frame_index++) {
                    dst[frame_index] = buffer[frame_index * channel_count + channel_index];
                }
            }
        }
        frames_read_total += frames_read;
    }

    if (frames_read_total < frame_count) {
        std::unique_ptr<Sample[]> trimmed_storage;
        if (frames_read_total > 0 && channel_count > 0) {
            trimmed_storage = std::make_unique<Sample[]>(frames_read_total * channel_count);
            for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
                std::copy_n(storage.get() + channel_index * frame_count,
                            frames_read_total,
                            trimmed_storage.get() + channel_index * frames_read_total);
            }
        }
        storage = std::move(trimmed_storage);
    }

    return std::make_unique<AudioData>(std::move(storage), frames_read_total, sample_rate, channel);
}

bool Lowl::Audio::AudioReaderOpus::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::OPUS;
}
