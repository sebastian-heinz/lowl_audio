#include "lowl_audio_reader_ogg.h"

#include "audio/lowl_audio_format.h"

#include <vorbis/vorbisfile.h>

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderOgg::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    OggVorbis_File vf;

    if (ov_open_callbacks(stdin, &vf, nullptr, 0, OV_CALLBACKS_NOCLOSE) < 0) {
        fprintf(stderr, "Input does not appear to be an Ogg bitstream.\n");
        exit(1);
    }

    vorbis_info *vi = ov_info(&vf, -1);

    int channel_count = vi->channels;
    ogg_int64_t sample_count = ov_pcm_total(&vf, -1);

    SampleFormat sample_format = SampleFormat::FLOAT_32;
    SampleRate sample_rate = (SampleRate) vi->rate;
    AudioChannel channel = get_channel(channel_count);

    size_t bytes_per_sample = get_sample_size(sample_format);
    AudioFormat audio_format = AudioFormat::OGG;

    int bitstream = 0;
    std::vector<float> samples = std::vector<float>();

    for (long readTotal = 0; readTotal < sample_count;) {
        float **pcm{};
        auto samples_read = ov_read_float(&vf, &pcm, (int) sample_count, &bitstream);

        for (int c = 0; c < channel_count; c++) {
            for (int s = 0; s < samples_read; s++) {
                samples.push_back(pcm[c][s]);
            }
        }
        readTotal += samples_read;
    }

    ov_clear(&vf);

    std::vector<AudioFrame> audio_frames = read_frames(channel, samples, error);
    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(audio_frames, sample_rate, channel);
    return audio_data;
}

bool Lowl::Audio::AudioReaderOgg::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::OGG;
}