#include "lowl_audio_reader_ogg.h"

#include "audio/lowl_audio_format.h"

#include <vorbis/vorbisfile.h>

struct OggData {
    uint8_t *data;
    size_t length;
    size_t index;
};

static size_t ogg_memory_read(void *buffer, size_t element_size, size_t element_count, void *source) {
    assert(element_size == 1);
    OggData *src = static_cast<OggData *>(source);
    uint8_t *dst = static_cast<uint8_t *>(buffer);
    size_t count = 0;
    size_t read = src->length - src->index;
    if (read > element_count) {
        read = element_count;
    }
    for (; count < read; count++) {
        dst[count] = src->data[src->index + count];
    }
    src->index += count;
    return count;
}

static int ogg_memory_seek(void *source, ogg_int64_t offset, int origin) {
    OggData *src = static_cast<OggData *>(source);
    if (origin == SEEK_SET) {
        /* set file offset to offset */
        src->index = (size_t) offset;
    } else if (origin == SEEK_CUR) {
        /* set file offset to current plus offset */
        src->index += (size_t) offset;
    } else if (origin == SEEK_END) {
        /* set file offset to EOF plus offset */
        src->index += src->length + (size_t) offset;
    }

    return 0;
}

static long ogg_memory_tell(void *source) {
    OggData *src = static_cast<OggData *>(source);
    return (long) src->index;
}


std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderOgg::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    OggVorbis_File vf;
    const ov_callbacks callbacks{
            &ogg_memory_read,
            &ogg_memory_seek,
            nullptr,
            &ogg_memory_tell
    };
    OggData ogg_data{
            p_buffer.get(),
            p_size,
            0
    };
    int ret = ov_open_callbacks(&ogg_data, &vf, nullptr, 0, callbacks);
    if (ret < 0) {
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
        for (int s = 0; s < samples_read; s++) {
            for (int c = 0; c < channel_count; c++) {
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