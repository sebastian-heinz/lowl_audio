#include "lowl_audio_reader_wav.h"

#include "lowl_audio_format.h"

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

std::unique_ptr<Lowl::AudioStream>
Lowl::AudioReaderWav::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    drwav wav;
    if (!drwav_init_memory(&wav, p_buffer.get(), p_size, nullptr)) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    /* Cannot use this function for compressed formats. */
    if (drwav__is_compressed_format_tag(wav.translatedFormatTag)) {
        // todo uninit?
        return nullptr;
    }

    uint32_t bytes_per_frame = drwav_get_bytes_per_pcm_frame(&wav);
    if (bytes_per_frame == 0) {
        return nullptr;
    }

    /* Don't try to read more samples than can potentially fit in the output buffer. */
    /* Intentionally uint64 instead of size_t so we can do a check that we're not reading too much on 32-bit builds. */
    uint64_t bytes_to_read_test = wav.totalPCMFrameCount * bytes_per_frame;
    if (bytes_to_read_test > DRWAV_SIZE_MAX) {
        /* Round the number of bytes to read to a clean frame boundary. */
        bytes_to_read_test = (DRWAV_SIZE_MAX / bytes_per_frame) * bytes_per_frame;
    }

    /*
    Doing an explicit check here just to make it clear that we don't want to be attempt to read anything if there's no bytes to read. There
    *could* be a time where it evaluates to 0 due to overflowing.
    */
    if (bytes_to_read_test == 0) {
        return nullptr;
    }
    size_t bytes_to_read = bytes_to_read_test;

    std::unique_ptr<uint8_t[]> pcm_frames = std::make_unique<uint8_t[]>(bytes_to_read);
    size_t bytes_read = drwav_read_raw(&wav, bytes_to_read, pcm_frames.get());

    size_t frames_read = bytes_read / bytes_per_frame;
    SampleRate sample_rate = wav.sampleRate;
    Channel channel = get_channel(wav.channels);
    size_t bytes_per_sample = bytes_per_frame / wav.channels;


    /* Don't try to read more samples than can potentially fit in the output buffer. */
    //if (framesToRead * pWav->channels * sizeof(float) > DRWAV_SIZE_MAX) {
    //    framesToRead = DRWAV_SIZE_MAX / sizeof(float) / pWav->channels;
    //}

    AudioFormat audio_format = AudioFormat::Unknown;
    SampleFormat sample_format = SampleFormat::Unknown;
    switch (wav.translatedFormatTag) {
        case DR_WAVE_FORMAT_PCM:
            audio_format = AudioFormat::WAVE_FORMAT_PCM;
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
        case DR_WAVE_FORMAT_ADPCM:
            audio_format = AudioFormat::WAVE_FORMAT_ADPCM;
            break;
        case DR_WAVE_FORMAT_IEEE_FLOAT:
            audio_format = AudioFormat::WAVE_FORMAT_IEEE_FLOAT;
            switch (bytes_per_sample) {
                case 4:
                    sample_format = SampleFormat::FLOAT_32;
                    break;
                case 8:
                    sample_format = SampleFormat::FLOAT_64;
                    break;
            }
            break;
        case DR_WAVE_FORMAT_ALAW:
            audio_format = AudioFormat::WAVE_FORMAT_ALAW;
            break;
        case DR_WAVE_FORMAT_MULAW:
            audio_format = AudioFormat::WAVE_FORMAT_MULAW;
            break;
        case DR_WAVE_FORMAT_DVI_ADPCM:
            audio_format = AudioFormat::WAVE_FORMAT_DVI_ADPCM;
            break;
    }

    std::vector<AudioFrame> audio_frames =
            read_frames(audio_format, sample_format, channel, std::move(pcm_frames), bytes_read, error);
    if (error.has_error()) {
        return nullptr;
    }

    std::unique_ptr<AudioStream> audio_stream = std::make_unique<AudioStream>(sample_rate, channel);
    audio_stream->write(audio_frames);

    drwav_uninit(&wav);
    return audio_stream;
}

bool Lowl::AudioReaderWav::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::WAV;
}