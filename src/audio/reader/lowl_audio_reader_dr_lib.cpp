#include "audio/reader/lowl_audio_reader_dr_lib.h"

#define DRWAV_API static
#define DRWAV_PRIVATE static
#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include <dr_wav.h>

#define DRFLAC_API static
#define DRFLAC_PRIVATE static
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#include <dr_flac.h>

#define DRMP3_API static
#define DRMP3_PRIVATE static
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>

namespace {
    Lowl::Audio::DrLib::WavFormatTag to_wav_format_tag(const drwav_uint16 p_format_tag) {
        switch (p_format_tag) {
            case DR_WAVE_FORMAT_PCM:
                return Lowl::Audio::DrLib::WavFormatTag::Pcm;
            case DR_WAVE_FORMAT_ADPCM:
                return Lowl::Audio::DrLib::WavFormatTag::Adpcm;
            case DR_WAVE_FORMAT_IEEE_FLOAT:
                return Lowl::Audio::DrLib::WavFormatTag::IeeeFloat;
            case DR_WAVE_FORMAT_ALAW:
                return Lowl::Audio::DrLib::WavFormatTag::Alaw;
            case DR_WAVE_FORMAT_MULAW:
                return Lowl::Audio::DrLib::WavFormatTag::Mulaw;
            case DR_WAVE_FORMAT_DVI_ADPCM:
                return Lowl::Audio::DrLib::WavFormatTag::DviAdpcm;
            default:
                return Lowl::Audio::DrLib::WavFormatTag::Unknown;
        }
    }
} // namespace

struct Lowl::Audio::DrLib::WavDecoder::Impl {
    drwav wav{};
    bool initialized = false;
    WavInfo info{};
};

Lowl::Audio::DrLib::WavDecoder::WavDecoder(const uint8_t *p_data, const size_t p_size) : impl(std::make_unique<Impl>()) {
    if (p_data == nullptr || !drwav_init_memory(&impl->wav, p_data, p_size, nullptr)) {
        return;
    }

    impl->initialized = true;
    impl->info.total_pcm_frame_count = impl->wav.totalPCMFrameCount;
    impl->info.bytes_per_pcm_frame = drwav_get_bytes_per_pcm_frame(&impl->wav);
    impl->info.format_tag = to_wav_format_tag(impl->wav.translatedFormatTag);
    impl->info.sample_rate = impl->wav.sampleRate;
    impl->info.channel_mask = impl->wav.fmt.channelMask;
    impl->info.channels = static_cast<uint16_t>(impl->wav.channels);
    impl->info.bits_per_sample = impl->wav.bitsPerSample;
}

Lowl::Audio::DrLib::WavDecoder::~WavDecoder() {
    if (impl && impl->initialized) {
        drwav_uninit(&impl->wav);
    }
}

Lowl::Audio::DrLib::WavDecoder::WavDecoder(WavDecoder &&p_other) noexcept = default;

Lowl::Audio::DrLib::WavDecoder &Lowl::Audio::DrLib::WavDecoder::operator=(WavDecoder &&p_other) noexcept = default;

bool Lowl::Audio::DrLib::WavDecoder::is_open() const {
    return impl && impl->initialized;
}

Lowl::Audio::DrLib::WavInfo Lowl::Audio::DrLib::WavDecoder::get_info() const {
    return is_open() ? impl->info : WavInfo{};
}

size_t Lowl::Audio::DrLib::WavDecoder::read_raw(const size_t p_bytes_to_read, void *p_dst) {
    if (!is_open() || p_dst == nullptr || p_bytes_to_read == 0) {
        return 0;
    }
    return drwav_read_raw(&impl->wav, p_bytes_to_read, p_dst);
}

struct Lowl::Audio::DrLib::FlacDecoder::Impl {
    drflac *flac = nullptr;
    FlacInfo info{};
};

Lowl::Audio::DrLib::FlacDecoder::FlacDecoder(const uint8_t *p_data, const size_t p_size)
    : impl(std::make_unique<Impl>()) {
    if (p_data == nullptr) {
        return;
    }

    impl->flac = drflac_open_memory(p_data, p_size, nullptr);
    if (impl->flac == nullptr) {
        return;
    }

    impl->info.total_pcm_frame_count = impl->flac->totalPCMFrameCount;
    impl->info.sample_rate = impl->flac->sampleRate;
    impl->info.channels = static_cast<uint8_t>(impl->flac->channels);
}

Lowl::Audio::DrLib::FlacDecoder::~FlacDecoder() {
    if (impl && impl->flac != nullptr) {
        drflac_close(impl->flac);
    }
}

Lowl::Audio::DrLib::FlacDecoder::FlacDecoder(FlacDecoder &&p_other) noexcept = default;

Lowl::Audio::DrLib::FlacDecoder &
Lowl::Audio::DrLib::FlacDecoder::operator=(FlacDecoder &&p_other) noexcept = default;

bool Lowl::Audio::DrLib::FlacDecoder::is_open() const {
    return impl && impl->flac != nullptr;
}

Lowl::Audio::DrLib::FlacInfo Lowl::Audio::DrLib::FlacDecoder::get_info() const {
    return is_open() ? impl->info : FlacInfo{};
}

size_t Lowl::Audio::DrLib::FlacDecoder::read_pcm_frames_s32(const size_t p_frames_to_read, int32_t *p_buffer_out) {
    if (!is_open() || p_buffer_out == nullptr || p_frames_to_read == 0) {
        return 0;
    }
    return static_cast<size_t>(drflac_read_pcm_frames_s32(impl->flac, p_frames_to_read, p_buffer_out));
}

struct Lowl::Audio::DrLib::Mp3Decoder::Impl {
    drmp3 mp3{};
    bool initialized = false;
    Mp3Info info{};
};

Lowl::Audio::DrLib::Mp3Decoder::Mp3Decoder(const uint8_t *p_data, const size_t p_size) : impl(std::make_unique<Impl>()) {
    if (p_data == nullptr || !drmp3_init_memory(&impl->mp3, p_data, p_size, nullptr)) {
        return;
    }

    impl->initialized = true;
    impl->info.total_pcm_frame_count = drmp3_get_pcm_frame_count(&impl->mp3);
    impl->info.sample_rate = impl->mp3.sampleRate;
    impl->info.channels = static_cast<uint8_t>(impl->mp3.channels);
}

Lowl::Audio::DrLib::Mp3Decoder::~Mp3Decoder() {
    if (impl && impl->initialized) {
        drmp3_uninit(&impl->mp3);
    }
}

Lowl::Audio::DrLib::Mp3Decoder::Mp3Decoder(Mp3Decoder &&p_other) noexcept = default;

Lowl::Audio::DrLib::Mp3Decoder &Lowl::Audio::DrLib::Mp3Decoder::operator=(Mp3Decoder &&p_other) noexcept = default;

bool Lowl::Audio::DrLib::Mp3Decoder::is_open() const {
    return impl && impl->initialized;
}

Lowl::Audio::DrLib::Mp3Info Lowl::Audio::DrLib::Mp3Decoder::get_info() const {
    return is_open() ? impl->info : Mp3Info{};
}

uint64_t Lowl::Audio::DrLib::Mp3Decoder::read_pcm_frames_f32(const uint64_t p_frames_to_read, float *p_buffer_out) {
    if (!is_open() || p_buffer_out == nullptr || p_frames_to_read == 0) {
        return 0;
    }
    return drmp3_read_pcm_frames_f32(&impl->mp3, p_frames_to_read, p_buffer_out);
}
