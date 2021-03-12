#include "lowl_audio_reader_wav.h"

#include <dr_wav.h>

std::unique_ptr<Lowl::AudioStream>
Lowl::AudioReaderWav::read_buffer(const std::unique_ptr<Buffer> &p_buffer, Error &error) {
    // WAV references:
    // http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
    // http://soundfile.sapp.org/doc/WaveFormat/

    bool is_riff = p_buffer->read_u8() == 'R' && p_buffer->read_u8() == 'I' && p_buffer->read_u8() == 'F' &&
                   p_buffer->read_u8() == 'F';
    if (!is_riff) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    // 36 + SubChunk2Size, or more precisely: 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
    // This is the size of the rest of the chunk following this number. This is the size of the
    // entire file in bytes minus 8 bytes for the two fields not included in this count: ChunkID and ChunkSize.
    uint32_t total_chunk_size = p_buffer->read_u32();

    bool is_wave = p_buffer->read_u8() == 'W' && p_buffer->read_u8() == 'A' && p_buffer->read_u8() == 'V' &&
                   p_buffer->read_u8() == 'E';
    if (!is_wave) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    Lowl::SampleFormat sample_format = Lowl::SampleFormat::Unknown;
    uint32_t sample_rate = 0;
    uint16_t num_channel = 0;
    size_t audio_size = 0;
    std::unique_ptr<uint8_t[]> audio_data = nullptr;

    bool has_fmt = false;
    bool has_data = false;
    bool has_fact = false;
    while (p_buffer->get_available() > 0) {
        char chunk_id[4];
        p_buffer->read_data((uint8_t *) &chunk_id, 4);

        uint32_t chunk_size = p_buffer->read_u32();
        uint32_t chunk_position = p_buffer->get_position();

        if (!has_fmt && chunk_id[0] == 'f' && chunk_id[1] == 'm' && chunk_id[2] == 't' && chunk_id[3] == ' ') {
            // PCM = 1 (i.e. Linear quantization) Values other than 1 indicate some form of compression.
            //0x0001 WAVE_FORMAT_PCM PCM
            //0x0003 WAVE_FORMAT_IEEE_FLOAT IEEE float
            //0x0006 WAVE_FORMAT_ALAW 8 - bit ITU - T G .711 A - law
            //0x0007 WAVE_FORMAT_MULAW 8 - bit ITU - T G .711 µ- law
            //0xFFFE WAVE_FORMAT_EXTENSIBLE Determined by SubFormat
            uint16_t audio_format = p_buffer->read_u16();
            switch (audio_format) {
                case 0x0001: {
                    break;
                }
                case 0x0003: {
                    break;
                }
                case 0x0006: {
                    break;
                }
                case 0x0007: {
                    break;
                }
                case 0xFFFE: {
                    break;
                }
                default: {
                    error.set_error(ErrorCode::WavReaderUnsupportedAudioFormat);
                    return nullptr;
                }
            }

            // Mono = 1, Stereo = 2, etc.
            num_channel = p_buffer->read_u16();

            // 8000, 44100, etc.
            sample_rate = p_buffer->read_u32();

            // == SampleRate * NumChannels * BitsPerSample/8
            uint32_t byte_range = p_buffer->read_u32();

            // == NumChannels * BitsPerSample/8 The number of bytes for one sample including all channels.
            uint16_t block_align = p_buffer->read_u16();

            // 8 bits = 8, 16 bits = 16, etc.
            uint16_t bits_per_sample = p_buffer->read_u16();

            if (bits_per_sample == 8) {
                sample_format = Lowl::SampleFormat::U_INT_8;
            } else if (bits_per_sample == 16) {
                sample_format = Lowl::SampleFormat::INT_16;
            } else if (audio_format == 3 && bits_per_sample == 32) {
                sample_format = Lowl::SampleFormat::FLOAT_32;
            } else {
                error.set_error(ErrorCode::Error);
               // return nullptr;
            }
            has_fmt = true;
        }

        if (!has_data && chunk_id[0] == 'd' && chunk_id[1] == 'a' && chunk_id[2] == 't' && chunk_id[3] == 'a') {
            audio_size = chunk_size;
            audio_data = std::make_unique<uint8_t[]>(audio_size);
            p_buffer->read_data(audio_data.get(), audio_size);
            has_data = true;
        }

        if (!has_fact && chunk_id[0] == 'f' && chunk_id[1] == 'a' && chunk_id[2] == 'c' && chunk_id[3] == 't') {
            has_fact = true;
        }

        p_buffer->seek(chunk_position + chunk_size);
    }

    Channel channel = get_channel(num_channel);
    if (channel == Channel::None) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    if (!audio_data) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    std::vector<AudioFrame> audio_frames = AudioReader::read_frames(sample_format, channel, audio_data.get(), audio_size);

    std::unique_ptr<AudioStream> audio_stream = std::make_unique<AudioStream>(sample_rate, channel);
    audio_stream->write(audio_frames);

    return audio_stream;
}
