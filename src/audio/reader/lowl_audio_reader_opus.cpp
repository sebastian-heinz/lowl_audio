#include "lowl_audio_reader_opus.h"

#include "audio/lowl_audio_format.h"

#include <opusfile.h>

#define OPUS_SAMPLE_RATE (48000)

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
    AudioFormat audio_format = AudioFormat::OGG;

    std::vector<float> buffer(2 * 48000 * 2, 0.0f);
    std::vector<AudioFrame> audio_frames = std::vector<AudioFrame>();

    for (;;) {
        int ret = op_read_float_stereo(ogg_file.get(), buffer.data(), int(buffer.size()));
        if (ret >= 0) {
            buffer.resize((size_t) ret);
            std::vector<AudioFrame> frames = read_frames(channel, buffer, error);
            audio_frames.insert(audio_frames.end(), frames.begin(), frames.end());
            if (ret == 0) {
                break;
            }
        } else {
            throw std::runtime_error("opusfile read error " + std::to_string(ret));
        }
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(audio_frames, sample_rate, channel);
    return audio_data;
}

bool Lowl::Audio::AudioReaderOpus::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::OPUS;
}