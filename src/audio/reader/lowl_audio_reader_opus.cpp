#include "lowl_audio_reader_opus.h"

#include <opusfile.h>

#include "audio/lowl_audio_format.h"
#include "audio/lowl_audio_utilities.h"

#define OPUS_SAMPLE_RATE (48000)
#define OPUS_BUFFER_SIZE_MS (500)

namespace {
    struct VorbisLayoutMapping {
        Lowl::Audio::ChannelLayout layout;
        std::vector<Lowl::Audio::Speaker> input_speakers;
    };

    VorbisLayoutMapping get_vorbis_layout_mapping(const uint8_t p_channel_count) {
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
} // namespace

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderOpus::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    int _error = 0;
    std::unique_ptr<OggOpusFile, decltype(&op_free)> ogg_file(op_open_memory(p_buffer.get(), p_size, &_error), op_free);

    if (ogg_file == nullptr) {
        return nullptr;
    }

    SampleRate sample_rate = OPUS_SAMPLE_RATE;
    uint32_t channel_count = static_cast<uint32_t>(op_channel_count(ogg_file.get(), -1));
    ChannelLayout layout = ChannelLayout::from_count(static_cast<uint8_t>(channel_count));
    std::vector<Speaker> input_speakers;
    const OpusHead *head = op_head(ogg_file.get(), -1);
    if (head != nullptr && head->mapping_family == 1) {
        const VorbisLayoutMapping mapping = get_vorbis_layout_mapping(static_cast<uint8_t>(channel_count));
        layout = mapping.layout;
        input_speakers = mapping.input_speakers;
    } else if (layout.is_valid()) {
        for (uint8_t channel_index = 0; channel_index < layout.channel_count; channel_index++) {
            input_speakers.push_back(layout.speaker_at(channel_index));
        }
    }
    if (!layout.is_valid()) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }
    ogg_int64_t sample_count = op_pcm_total(ogg_file.get(), -1);
    const size_t frame_count = sample_count > 0 ? static_cast<size_t>(sample_count) : 0;
    size_t buffer_size = Lowl::Audio::ms_to_samples(OPUS_BUFFER_SIZE_MS, sample_rate, layout.channel_count);
    std::vector<float> buffer(buffer_size, 0.0f);
    std::unique_ptr<Sample[]> storage;
    if (frame_count > 0 && layout.channel_count > 0) {
        storage = std::make_unique<Sample[]>(frame_count * layout.channel_count);
    }
    size_t frames_read_total = 0;

    std::vector<int> source_indices(layout.channel_count, -1);
    for (uint8_t channel_index = 0; channel_index < layout.channel_count; channel_index++) {
        const Speaker speaker = layout.speaker_at(channel_index);
        for (size_t input_index = 0; input_index < input_speakers.size(); input_index++) {
            if (input_speakers[input_index] == speaker) {
                source_indices[channel_index] = static_cast<int>(input_index);
                break;
            }
        }
        if (source_indices[channel_index] < 0) {
            error.set_error(ErrorCode::UnsupportedAudioFormat);
            return nullptr;
        }
    }

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
        for (uint8_t channel_index = 0; channel_index < layout.channel_count; channel_index++) {
            const size_t source_channel_index = static_cast<size_t>(source_indices[channel_index]);
            Sample *dst = storage ? storage.get() + channel_index * frame_count + frames_read_total : nullptr;
            if (dst != nullptr) {
                for (size_t frame_index = 0; frame_index < frames_read; frame_index++) {
                    dst[frame_index] = buffer[frame_index * channel_count + source_channel_index];
                }
            }
        }
        frames_read_total += frames_read;
    }

    if (frames_read_total < frame_count) {
        std::unique_ptr<Sample[]> trimmed_storage;
        if (frames_read_total > 0 && layout.channel_count > 0) {
            trimmed_storage = std::make_unique<Sample[]>(frames_read_total * layout.channel_count);
            for (uint8_t channel_index = 0; channel_index < layout.channel_count; channel_index++) {
                std::copy_n(storage.get() + channel_index * frame_count,
                            frames_read_total,
                            trimmed_storage.get() + channel_index * frames_read_total);
            }
        }
        storage = std::move(trimmed_storage);
    }

    return std::make_unique<AudioData>(std::move(storage), frames_read_total, sample_rate, layout);
}

bool Lowl::Audio::AudioReaderOpus::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::OPUS;
}
