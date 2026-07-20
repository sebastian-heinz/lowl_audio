#include "lowl_audio_reader_wav.h"

#include <cstdint>
#include <limits>

#include "audio/reader/lowl_audio_reader_dr_lib.h"

std::unique_ptr<Lowl::Audio::AudioData>
Lowl::Audio::AudioReaderWav::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {
    DrLib::WavDecoder wav(p_buffer.get(), p_size);
    if (!wav.is_open()) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    const DrLib::WavInfo wav_info = wav.get_info();
    /* Cannot use this function for compressed formats. */
    if (wav_info.format_tag == DrLib::WavFormatTag::Adpcm || wav_info.format_tag == DrLib::WavFormatTag::DviAdpcm) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    uint32_t bytes_per_frame = wav_info.bytes_per_pcm_frame;
    if (bytes_per_frame == 0) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    /* Don't try to read more samples than can potentially fit in the output buffer. */
    /* Intentionally uint64 instead of size_t so we can do a check that we're not reading too much on 32-bit builds. */
    uint64_t bytes_to_read_test =
        wav_info.total_pcm_frame_count * static_cast<uint64_t>(bytes_per_frame);
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

    std::unique_ptr<uint8_t[]> pcm_frames = std::make_unique<uint8_t[]>(bytes_to_read);
    size_t bytes_read = wav.read_raw(bytes_to_read, pcm_frames.get());

    SampleRate sample_rate = wav_info.sample_rate;
    const ChannelLayout layout = wav_info.channel_mask != 0
                                     ? ChannelLayout::from_mask(wav_info.channel_mask)
                                     : ChannelLayout::from_count(static_cast<uint8_t>(wav_info.channels));
    if (!layout.is_valid()) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }
    size_t bytes_per_sample = wav_info.channels > 0 ? bytes_per_frame / wav_info.channels : 0;

    SampleFormat sample_format = SampleFormat::Unknown;
    switch (wav_info.format_tag) {
        case DrLib::WavFormatTag::Pcm:
            switch (bytes_per_sample) {
                case 4:
                    sample_format = SampleFormat::INT_32;
                    break;
                case 3:
                    sample_format = SampleFormat::INT_24;
                    break;
                case 2:
                    sample_format = SampleFormat::INT_16;
                    break;
                case 1:
                    sample_format = SampleFormat::U_INT_8;
                    break;
            }
            break;
        case DrLib::WavFormatTag::Adpcm:
            error.set_error(ErrorCode::UnsupportedAudioFormat);
            return nullptr;
        case DrLib::WavFormatTag::IeeeFloat:
            switch (bytes_per_sample) {
                case 4:
                    sample_format = SampleFormat::FLOAT_32;
                    break;
                case 8:
                    sample_format = SampleFormat::FLOAT_64;
                    break;
            }
            break;
        case DrLib::WavFormatTag::Alaw:
        case DrLib::WavFormatTag::Mulaw:
        case DrLib::WavFormatTag::DviAdpcm:
        case DrLib::WavFormatTag::Unknown:
            error.set_error(ErrorCode::UnsupportedAudioFormat);
            return nullptr;
    }

    if (sample_format == SampleFormat::Unknown) {
        error.set_error(ErrorCode::UnsupportedAudioFormat);
        return nullptr;
    }

    std::unique_ptr<AudioData> audio_data =
        create_audio_data(sample_format, layout, sample_rate, pcm_frames, bytes_read, {}, error);
    return audio_data;
}

bool Lowl::Audio::AudioReaderWav::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::WAV;
}
