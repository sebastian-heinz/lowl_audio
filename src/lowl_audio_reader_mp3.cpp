#include "lowl_audio_reader_mp3.h"

#include "lowl_audio_format.h"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_FLOAT_OUTPUT

#include <dr_mp3.h>

#define ENCODED_BUFFER_DECODING_STEP (16384)
#define DECODED_BUFFER_SIZE (ENCODED_BUFFER_DECODING_STEP*32*8)

std::unique_ptr<Lowl::AudioData>
Lowl::AudioReaderMp3::read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Error &error) {

    size_t bytes_read = 0;
    const drmp3_uint8 *mp3_buffer = p_buffer.get();
    SampleFormat sample_format = SampleFormat::FLOAT_32;
    AudioFormat audio_format = AudioFormat::MP3;
    size_t bytes_per_sample = get_sample_size(sample_format);
    std::unique_ptr<uint8_t[]> pcm_buffer = std::make_unique<uint8_t[]>(DECODED_BUFFER_SIZE);
    drmp3dec_frame_info frame_info;
    drmp3dec decoder;

    // read first frame to get channel & sample rate
    drmp3dec_init(&decoder);
    size_t pcm_frames_read = drmp3dec_decode_frame(
            &decoder, &mp3_buffer[bytes_read], ENCODED_BUFFER_DECODING_STEP, pcm_buffer.get(), &frame_info
    );
    Channel channel = get_channel(frame_info.channels);
    size_t bytes_per_frame = bytes_per_sample * get_channel_num(channel);
    SampleRate sample_rate = frame_info.hz;
    bytes_read += frame_info.frame_bytes;
    size_t pcm_buffer_size = pcm_frames_read * bytes_per_frame;

    std::vector<AudioFrame> audio_frames = read_frames(
            audio_format, sample_format, channel, pcm_buffer, pcm_buffer_size, error
    );
    if (error.has_error()) {
        return nullptr;
    }

    // read remaining frames
    while (bytes_read <= p_size) {
        pcm_frames_read = drmp3dec_decode_frame(
                &decoder, &mp3_buffer[bytes_read], ENCODED_BUFFER_DECODING_STEP, pcm_buffer.get(), &frame_info
        );
        bytes_read += frame_info.frame_bytes;
        pcm_buffer_size = pcm_frames_read * bytes_per_frame;
        std::vector<AudioFrame> frames = read_frames(
                audio_format, sample_format, channel, pcm_buffer, pcm_buffer_size, error
        );
        if (error.has_error()) {
            return nullptr;
        }
        audio_frames.insert(audio_frames.end(), frames.begin(), frames.end());
    }

    std::unique_ptr<AudioData> audio_data = std::make_unique<AudioData>(audio_frames, sample_rate, channel);
    return audio_data;
}

bool Lowl::AudioReaderMp3::support(Lowl::FileFormat p_file_format) const {
    return p_file_format == FileFormat::MP3;
}