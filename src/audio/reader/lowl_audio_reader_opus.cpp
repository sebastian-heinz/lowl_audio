#include "lowl_audio_reader_opus.h"

#include "audio/lowl_audio_format.h"
#include "audio/lowl_audio_utilities.h"

#include <opusfile.h>

#define OPUS_SAMPLE_RATE (48000)
#define OPUS_BUFFER_SIZE_MS (500)

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderOpus::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    int _error = 0;
    std::unique_ptr<OggOpusFile, decltype(&op_free)> ogg_file(
            op_open_memory(p_buffer.get(), p_size, &_error),
            op_free
    );

    if (ogg_file == nullptr) {
        return nullptr;
    }

    SampleRate sample_rate = OPUS_SAMPLE_RATE;
    SampleFormat sample_format = SampleFormat::FLOAT_32;
    int channel_count = op_channel_count(ogg_file.get(), -1);
    AudioChannel channel = get_channel(channel_count);
    ogg_int64_t sample_count = op_pcm_total(ogg_file.get(), -1);
    size_t bytes_per_sample = get_sample_size(sample_format);
    AudioFormat audio_format = AudioFormat::OPUS;
    size_t buffer_size = Lowl::Audio::ms_to_samples(OPUS_BUFFER_SIZE_MS, sample_rate, channel);
    std::vector<float> buffer(buffer_size, 0.0f);
    std::vector<float> samples = std::vector<float>();

    for (long total_samples_read = 0; total_samples_read < sample_count;) {
        int samples_read_per_channel = op_read_float(ogg_file.get(), buffer.data(), (int) buffer.size(), nullptr);
        if (samples_read_per_channel < 0) {
            error.set_error(ErrorCode::OpusFileCanNotParseOpusFile);
            return nullptr;
        }
        uint64_t samples_read = (uint64_t) (samples_read_per_channel * channel_count);
        for (uint64_t s = 0; s < samples_read; s++) {
            samples.push_back(buffer[s]);
        }
        total_samples_read += samples_read;
    }

    std::vector<AudioFrame> audio_frames = read_frames(channel, samples, error);
    if (error.has_error()) {
        return nullptr;
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(audio_frames, sample_rate, channel);
    return audio_data;
}

bool Lowl::Audio::AudioReaderOpus::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::OPUS;
}