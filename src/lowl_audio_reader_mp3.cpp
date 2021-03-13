#include "lowl_audio_reader_mp3.h"

#include "lowl_audio_format.h"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT

#include <dr_mp3.h>

std::unique_ptr<Lowl::AudioStream>
Lowl::AudioReaderMp3::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    drmp3 mp3;
    if (!drmp3_init_memory(&mp3, p_buffer.get(), p_size, nullptr)) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    Channel channel = get_channel(mp3.channels);
    SampleRate sample_rate = mp3.sampleRate;
    size_t num_frames = mp3.pcmFramesRemainingInMP3Frame;
    SampleFormat sample_format = SampleFormat::FLOAT_32;
    AudioFormat audio_format = AudioFormat::MP3;
    size_t bytes_per_sample = get_sample_size(sample_format);
    size_t bytes_per_frame = bytes_per_sample * get_channel_num(channel);

    /* Don't try to read more samples than can potentially fit in the output buffer. */
    /* Intentionally uint64 instead of size_t so we can do a check that we're not reading too much on 32-bit builds. */
    uint64_t bytes_to_read_test = num_frames * bytes_per_frame;
    if (bytes_to_read_test > DRMP3_SIZE_MAX) {
        /* Round the number of bytes to read to a clean frame boundary. */
        bytes_to_read_test = (DRMP3_SIZE_MAX / bytes_per_frame) * bytes_per_frame;
    }

    /*
    Doing an explicit check here just to make it clear that we don't want to be attempt to read anything if there's no bytes to read. There
    *could* be a time where it evaluates to 0 due to overflowing.
    */
    if (bytes_to_read_test == 0) {
        // todo uninit drwav?
        return nullptr;
    }
    size_t bytes_to_read = bytes_to_read_test;

    std::unique_ptr<uint8_t[]> pcm_frames = std::make_unique<uint8_t[]>(bytes_to_read);
    size_t frames_read = drmp3_read_pcm_frames_raw(&mp3, bytes_to_read, pcm_frames.get());
    drmp3_uninit(&mp3);

    size_t bytes_read = frames_read * bytes_per_frame;

    std::vector<AudioFrame> audio_frames =
            read_frames(audio_format, sample_format, channel, std::move(pcm_frames), bytes_read, error);
    if (error.has_error()) {
        return nullptr;
    }

    std::unique_ptr<AudioStream> audio_stream = std::make_unique<AudioStream>(sample_rate, channel);
    audio_stream->write(audio_frames);
    return audio_stream;
}

bool Lowl::AudioReaderMp3::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::MP3;
}