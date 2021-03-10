#ifndef LOWL_H
#define LOWL_H

#include "../src/lowl_audio_reader.h"
#include "../src/lowl_audio_stream.h"
#include "../src/lowl_driver.h"
#include "../src/lowl_file_format.h"

#include <vector>

namespace Lowl {

    std::vector<Driver *> get_drivers(Error &error);

    void initialize(Error &error);

    void terminate(Error &error);

    std::unique_ptr<AudioStream>
    create_stream(void *p_buffer, uint32_t p_length, FileFormat p_format, Error &error);

    std::unique_ptr<AudioStream> create_stream(const std::string &p_path, Error &error);

    std::unique_ptr<AudioReader> create_reader(FileFormat p_format, Error &error);

    FileFormat detect_format(const std::string &p_path, Error &error);
}
#endif /* LOWL_H */