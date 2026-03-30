#include "lowl_audio_reader_flac.h"

#include <cstdint>
#include <cstring>
#include <limits>

#include "audio/lowl_audio_format.h"
#include "audio/reader/lowl_audio_reader_dr_lib.h"

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
Lowl::Audio::AudioReaderFlac::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {
    DrLib::FlacDecoder flac(p_buffer.get(), p_size);
    if (!flac.is_open()) {
        error.set_error(ErrorCode::ReaderNotFound);
        return nullptr;
    }

    const DrLib::FlacInfo flac_info = flac.get_info();
    SampleFormat sample_format = SampleFormat::INT_32;
    AudioFormat audio_format = AudioFormat::FLAC;
    size_t bytes_per_sample = get_sample_size_bytes(sample_format);
    const VorbisLayoutMapping mapping = get_vorbis_layout_mapping(flac_info.channels);
    if (!mapping.layout.is_valid()) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }
    size_t bytes_per_frame = bytes_per_sample * mapping.layout.channel_count;
    SampleRate sample_rate = flac_info.sample_rate;

    /* Don't try to read more samples than can potentially fit in the output buffer. */
    /* Intentionally uint64 instead of size_t so we can do a check that we're not reading too much on 32-bit builds. */
    uint64_t bytes_to_read_test =
        flac_info.total_pcm_frame_count * static_cast<uint64_t>(bytes_per_frame);
#if SIZE_MAX < UINT64_MAX
    if (bytes_to_read_test > static_cast<uint64_t>(SIZE_MAX)) {
        /* Round the number of bytes to read to a clean frame boundary. */
        bytes_to_read_test = (std::numeric_limits<size_t>::max() / bytes_per_frame) * bytes_per_frame;
    }
#endif

    /*
    Doing an explicit check here just to make it clear that we don't want to be attempt to read anything if there's no bytes to read. There
    *could* be a time where it evaluates to 0 due to overflowing.
    */
    if (bytes_to_read_test == 0) {
        error.set_error(ErrorCode::ReaderNoAudioData);
        return nullptr;
    }
    size_t bytes_to_read = bytes_to_read_test;
    const size_t frame_capacity = bytes_to_read / bytes_per_frame;

    std::vector<int32_t> decoded_frames;
    if (frame_capacity > 0 && mapping.layout.channel_count > 0) {
        decoded_frames.resize(frame_capacity * mapping.layout.channel_count);
    }
    const size_t pcm_frames_read =
        decoded_frames.empty() ? 0 : flac.read_pcm_frames_s32(frame_capacity, decoded_frames.data());
    size_t pcm_buffer_size = pcm_frames_read * bytes_per_frame;
    std::unique_ptr<uint8_t[]> pcm_frames;
    if (pcm_buffer_size > 0) {
        pcm_frames = std::make_unique<uint8_t[]>(pcm_buffer_size);
        std::memcpy(pcm_frames.get(), decoded_frames.data(), pcm_buffer_size);
    }

    return create_audio_data(
        audio_format, sample_format, mapping.layout, sample_rate, pcm_frames, pcm_buffer_size, mapping.input_speakers, error);
}

bool Lowl::Audio::AudioReaderFlac::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::FLAC;
}
