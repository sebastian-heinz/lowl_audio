#include "lowl_audio_reader_ogg.h"

#include <vorbis/vorbisfile.h>

#include <algorithm>
#include <cstdint>

#include "audio/lowl_audio_format.h"

struct OggData {
    uint8_t *data;
    size_t length;
    size_t index;
};

struct VorbisLayoutMapping {
    Lowl::Audio::ChannelLayout layout;
    std::vector<Lowl::Audio::Speaker> input_speakers;
};

static VorbisLayoutMapping get_vorbis_layout_mapping(const uint8_t p_channel_count) {
    using Lowl::Audio::ChannelLayout;
    using Lowl::Audio::Speaker;

    switch (p_channel_count) {
        case 1:
            return {ChannelLayout::Mono, {Speaker::FrontCenter}};
        case 2:
            return {ChannelLayout::Stereo, {Speaker::FrontLeft, Speaker::FrontRight}};
        case 3:
            return {ChannelLayout::Surround_3_0, {Speaker::FrontLeft, Speaker::FrontCenter, Speaker::FrontRight}};
        case 4:
            return {ChannelLayout::Quad, {Speaker::FrontLeft, Speaker::FrontRight, Speaker::BackLeft, Speaker::BackRight}};
        case 5:
            return {ChannelLayout::Surround_5_0_Rear,
                    {Speaker::FrontLeft, Speaker::FrontCenter, Speaker::FrontRight, Speaker::BackLeft, Speaker::BackRight}};
        case 6:
            return {ChannelLayout::Surround_5_1_Rear,
                    {Speaker::FrontLeft,
                     Speaker::FrontCenter,
                     Speaker::FrontRight,
                     Speaker::BackLeft,
                     Speaker::BackRight,
                     Speaker::LowFrequency}};
        case 7:
            return {ChannelLayout::Surround_6_1,
                    {Speaker::FrontLeft,
                     Speaker::FrontCenter,
                     Speaker::FrontRight,
                     Speaker::SideLeft,
                     Speaker::SideRight,
                     Speaker::BackCenter,
                     Speaker::LowFrequency}};
        case 8:
            return {ChannelLayout::Surround_7_1,
                    {Speaker::FrontLeft,
                     Speaker::FrontCenter,
                     Speaker::FrontRight,
                     Speaker::SideLeft,
                     Speaker::SideRight,
                     Speaker::BackLeft,
                     Speaker::BackRight,
                     Speaker::LowFrequency}};
        default:
            return {ChannelLayout::from_count(p_channel_count), {}};
    }
}

static size_t ogg_memory_read(void *buffer, size_t element_size, size_t element_count, void *source) {
    if (element_size == 0 || element_count == 0) {
        return 0;
    }
    OggData *src = static_cast<OggData *>(source);
    uint8_t *dst = static_cast<uint8_t *>(buffer);
    const size_t available_bytes = src->length - src->index;
    const size_t item_count = std::min(element_count, available_bytes / element_size);
    const size_t bytes_to_read = item_count * element_size;
    for (size_t count = 0; count < bytes_to_read; count++) {
        dst[count] = src->data[src->index + count];
    }
    src->index += bytes_to_read;
    return item_count;
}

static int ogg_memory_seek(void *source, ogg_int64_t offset, int origin) {
    OggData *src = static_cast<OggData *>(source);
    int64_t target = 0;
    if (origin == SEEK_SET) {
        target = offset;
    } else if (origin == SEEK_CUR) {
        target = static_cast<int64_t>(src->index) + offset;
    } else if (origin == SEEK_END) {
        target = static_cast<int64_t>(src->length) + offset;
    } else {
        return -1;
    }
    src->index = static_cast<size_t>(std::clamp<int64_t>(target, 0, static_cast<int64_t>(src->length)));
    return 0;
}

static long ogg_memory_tell(void *source) {
    OggData *src = static_cast<OggData *>(source);
    return (long)src->index;
}

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderOgg::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    OggVorbis_File vf;
    const ov_callbacks callbacks{&ogg_memory_read, &ogg_memory_seek, nullptr, &ogg_memory_tell};
    OggData ogg_data{p_buffer.get(), p_size, 0};
    int ret = ov_open_callbacks(&ogg_data, &vf, nullptr, 0, callbacks);
    if (ret < 0) {
        ov_clear(&vf);
        error.set_error(ErrorCode::VorbisFileInvalidOggFile);
        return nullptr;
    }

    vorbis_info *vi = ov_info(&vf, -1);

    uint32_t channel_count = static_cast<uint32_t>(vi->channels);
    ogg_int64_t sample_count = ov_pcm_total(&vf, -1);

    const VorbisLayoutMapping mapping = get_vorbis_layout_mapping(static_cast<uint8_t>(channel_count));
    if (!mapping.layout.is_valid()) {
        ov_clear(&vf);
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    std::vector<int> source_indices(mapping.layout.channel_count, -1);
    for (uint8_t channel_index = 0; channel_index < mapping.layout.channel_count; channel_index++) {
        const Lowl::Audio::Speaker speaker = mapping.layout.speaker_at(channel_index);
        for (size_t input_index = 0; input_index < mapping.input_speakers.size(); input_index++) {
            if (mapping.input_speakers[input_index] == speaker) {
                source_indices[channel_index] = static_cast<int>(input_index);
                break;
            }
        }
        if (source_indices[channel_index] < 0) {
            ov_clear(&vf);
            error.set_error(ErrorCode::UnsupportedAudioFormat);
            return nullptr;
        }
    }

    SampleRate sample_rate = (SampleRate)vi->rate;
    const size_t frame_count = sample_count > 0 ? static_cast<size_t>(sample_count) : 0;
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0 && mapping.layout.channel_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * mapping.layout.channel_count);
    }

    int bitstream = 0;
    size_t frames_read_total = 0;
    for (size_t read_total = 0; read_total < frame_count;) {
        float **pcm{};
        const auto samples_read = ov_read_float(&vf, &pcm, static_cast<int>(frame_count - read_total), &bitstream);
        if (samples_read < 0) {
            ov_clear(&vf);
            error.set_error(ErrorCode::VorbisFileCanNotParseOggFile);
            return nullptr;
        }
        if (samples_read == 0) {
            break;
        }
        for (uint8_t channel_index = 0; channel_index < mapping.layout.channel_count; channel_index++) {
            const size_t source_channel_index = static_cast<size_t>(source_indices[channel_index]);
            Sample *dst = storage ? storage.get() + static_cast<size_t>(channel_index) * frame_count + read_total : nullptr;
            if (dst != nullptr) {
                std::copy_n(pcm[source_channel_index], static_cast<size_t>(samples_read), dst);
            }
        }
        read_total += static_cast<size_t>(samples_read);
        frames_read_total = read_total;
    }
    ov_clear(&vf);
    if (frames_read_total < frame_count) {
        std::unique_ptr<Sample[]> trimmed_storage;
        if (frames_read_total > 0 && mapping.layout.channel_count > 0) {
            trimmed_storage = std::make_unique<Sample[]>(frames_read_total * mapping.layout.channel_count);
            for (uint8_t channel_index = 0; channel_index < mapping.layout.channel_count; channel_index++) {
                std::copy_n(storage.get() + channel_index * frame_count,
                            frames_read_total,
                            trimmed_storage.get() + channel_index * frames_read_total);
            }
        }
        storage = std::move(trimmed_storage);
    }

    return std::make_unique<AudioData>(std::move(storage), frames_read_total, sample_rate, mapping.layout);
}

bool Lowl::Audio::AudioReaderOgg::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::OGG;
}
