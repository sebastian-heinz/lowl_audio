#include "../include/lowl_audio_reader.h"

#include "lowl_file.h"

std::unique_ptr<LowlAudioStream> LowlAudioReader::read_ptr(void *p_buffer, uint32_t p_length, LowlError &error) {
    std::unique_ptr<LowlBuffer> buffer = std::make_unique<LowlBuffer>();
    buffer->write_data(p_buffer, p_length);
    buffer->seek(0);
    std::unique_ptr<LowlAudioStream> stream = read_buffer(buffer, error);
    if (error.has_error()) {
        return nullptr;
    }
    if (!stream) {
        error.set_error(LowlError::Code::Error);
        return nullptr;
    }
    return stream;
}

std::unique_ptr<LowlAudioStream> LowlAudioReader::read_file(const std::string &p_path, LowlError &error) {
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
    std::unique_ptr<LowlAudioStream> stream = read_ptr(buffer, length, error);
    free(buffer);
    if (error.has_error()) {
        return nullptr;
    }
    if (!stream) {
        error.set_error(LowlError::Code::Error);
        return nullptr;
    }
    return stream;
}

LowlAudioReader::LowlAudioReader() {
    file_path = std::string();
    file_extension = std::string();
}

LowlAudioReader::~LowlAudioReader() {
}
