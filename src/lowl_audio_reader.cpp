#include "lowl_audio_reader.h"

#include "lowl_file.h"
#include "lowl_buffer.h"


std::unique_ptr<Lowl::AudioStream> Lowl::AudioReader::read_ptr(void *p_buffer, uint32_t p_length, Error &error) {
    std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>();
    buffer->write_data(p_buffer, p_length);
    buffer->seek(0);
    std::unique_ptr<AudioStream> stream = read_buffer(buffer, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!stream) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    return stream;
}

std::unique_ptr<Lowl::AudioStream> Lowl::AudioReader::read_file(const std::string &p_path, Error &error) {
    LowlFile *file = new LowlFile();
    file->open(p_path, error);
    if (error.has_error()) {
        delete file;
        return nullptr;
    }
    uint32_t length = file->get_length();
    uint8_t *buffer = (uint8_t *) malloc(length);
    file->get_buffer(buffer, length);
    delete file;
    std::unique_ptr<AudioStream> stream = read_ptr(buffer, length, error);
    free(buffer);
    if (error.has_error()) {
        return nullptr;
    }
    if (!stream) {
        error.set_error(ErrorCode::Error);
        return nullptr;
    }
    return stream;
}

std::vector<Lowl::AudioFrame>
Lowl::AudioReader::read_frames(SampleFormat format, Channel channel, void *data, size_t data_size) {

    std::vector<AudioFrame> frames = std::vector<AudioFrame>();
    int sample_size = Lowl::get_sample_size(format);
    int sample_num = data_size / sample_size;
    int expected_data_size = sample_num * sample_size;
    if (expected_data_size != data_size) {
        // incomplete frames
    }
    int num_channels = get_channel_num(channel);
    int num_frames = sample_num / num_channels;

    switch (format) {
        case SampleFormat::FLOAT_32: {
            float *float32 = static_cast<float *>(data);
            for (int current_frame = 0; current_frame < num_frames; current_frame++) {
                int sample_index = current_frame * 2;
                AudioFrame frame{};
                frame.left = float32[sample_index];
                frame.right = float32[sample_index + 1];
                frames.push_back(frame);
            }
            break;
        }
        case SampleFormat::INT_32: {
            int32_t *int32 = static_cast<int32_t *>(data);
            break;
        }
        case SampleFormat::INT_24: {
            int32_t *int24 = static_cast<int32_t *>(data);
            break;
        }
        case SampleFormat::INT_16: {
            int16_t *int16 = static_cast<int16_t *>(data);
            switch (channel) {
                case Channel::Mono: {
                    for (int current_frame = 0; current_frame < num_frames; current_frame++) {
                        int sample_index = current_frame * 1;
                        int16_t center = int16[sample_index];
                        AudioFrame frame{};
                        frame.left = frame.right = sample_to_float(center);
                        frames.push_back(frame);
                    }
                    break;
                }
                case Channel::Stereo: {
                    for (int current_frame = 0; current_frame < num_frames; current_frame++) {
                        int sample_index = current_frame * 2;
                        int16_t left = int16[sample_index];
                        int16_t right = int16[sample_index + 1];
                        AudioFrame frame{};
                        frame.left = sample_to_float(left);
                        frame.right = sample_to_float(right);
                        frames.push_back(frame);
                    }
                    break;
                }
            }
            break;
        }
        case SampleFormat::INT_8: {
            break;
        }
        case SampleFormat::U_INT_8: {
            break;
        }
        case SampleFormat::Unknown: {
            break;
        }
    }
    return frames;
}
