#include "lowl_file.h"


std::string Lowl::LowlFile::get_path() {
    return path;
}

void Lowl::LowlFile::open(const std::string &p_path, Lowl::Error &error) {
    close();
    file_stream = std::make_unique<std::ifstream>(p_path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if (!file_stream) {
        close();
        error.set_error(ErrorCode::Error);
        return;
    }
    std::ifstream::pos_type size = file_stream->tellg();
    file_size = size;
    file_stream->seekg(0, std::ios::beg);
    path = p_path;
}

void Lowl::LowlFile::close() {
    if (file_stream) {
        file_stream->close();
    }
    file_stream = nullptr;
    path = std::string();
    file_size = 0;
}

bool Lowl::LowlFile::seek(size_t p_position) {
    if (!file_stream) {
        return false;
    }
    file_stream->seekg(p_position, std::ios::beg);
    if (file_stream->fail()) {
        return false;
    }
    if (file_stream->bad()) {
        return false;
    }
    return true;
}

bool Lowl::LowlFile::get_position(size_t &position) const {
    if (!file_stream) {
        return false;
    }
    std::streampos stream_position = file_stream->tellg();
    if (stream_position == -1) {
        position = 0;
        return false;
    }
    position = stream_position;
    return true;
}

size_t Lowl::LowlFile::get_length() const {
    return file_size;
}

uint8_t Lowl::LowlFile::read_u8() const {
    if (!file_stream) {
        return 0;
    }
    uint8_t b;
    file_stream->read(reinterpret_cast<char *>(&b), 1);
    return b;
}

std::unique_ptr<uint8_t[]> Lowl::LowlFile::read_buffer(size_t &length) const {
    std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(length);
    if (!file_stream) {
        length = 0;
        return data;
    }
    file_stream->read(reinterpret_cast<char *>(data.get()), length);
    if (file_stream->eof()) {
        length = file_stream->gcount();
        // The function stopped extracting characters because the input sequence has no more characters available (end-of-file reached).
    }
    if (file_stream->fail()) {
        length = 0;
        // Either the function could not extract n characters or the construction of sentry failed.
    }
    if (file_stream->bad()) {
        length = 0;
        // Error on stream (such as when this function catches an exception thrown by an internal operation).
        // When set, the integrity of the stream may have been affected.
    }
    return data;
}

bool Lowl::LowlFile::is_eof() const {
    if (!file_stream) {
        return false;
    }
    return file_stream->eof();
}

Lowl::LowlFile::LowlFile() {
    path = std::string();
    file_stream = nullptr;
    file_size = 0;
}

