#include "lowl_audio_reader_flac.h"

#include "lowl_audio_format.h"

#define DR_FLAC_IMPLEMENTATION

#include <dr_flac.h>


std::unique_ptr<Lowl::AudioData>
Lowl::AudioReaderFlac::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    drflac *flac = drflac_open_memory(p_buffer.get(), p_size, nullptr);
    if (!flac) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    SampleFormat sample_format = SampleFormat::INT_32;
    AudioFormat audio_format = AudioFormat::FLAC;
    size_t bytes_per_sample = get_sample_size(sample_format);
    Channel channel = get_channel(flac->channels);
    size_t bytes_per_frame = bytes_per_sample * get_channel_num(channel);
    SampleRate sample_rate = flac->sampleRate;

    /* Don't try to read more samples than can potentially fit in the output buffer. */
    /* Intentionally uint64 instead of size_t so we can do a check that we're not reading too much on 32-bit builds. */
    uint64_t bytes_to_read_test = flac->totalPCMFrameCount * bytes_per_frame;
    if (bytes_to_read_test > DRFLAC_SIZE_MAX) {
        /* Round the number of bytes to read to a clean frame boundary. */
        bytes_to_read_test = (DRFLAC_SIZE_MAX / bytes_per_frame) * bytes_per_frame;
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
    int32_t *buffer = reinterpret_cast<int32_t *>(pcm_frames.get());
    size_t pcm_frames_read = drflac_read_pcm_frames_s32(flac, flac->totalPCMFrameCount, buffer);
    size_t pcm_buffer_size = pcm_frames_read * bytes_per_frame;

    std::vector<AudioFrame> audio_frames =
            read_frames(audio_format, sample_format, channel, pcm_frames, pcm_buffer_size, error);
    if (error.has_error()) {
        return nullptr;
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(audio_frames, sample_rate, channel);
    return audio_data;
}

bool Lowl::AudioReaderFlac::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::FLAC;
}