#ifndef LOWL_H
#define LOWL_H

#include "../src/lowl_audio_reader.h"
#include "../src/lowl_audio_stream.h"
#include "../src/lowl_driver.h"
#include "../src/lowl_file_format.h"

#include <vector>

namespace Lowl {

    std::vector<Driver *> get_drivers(LowlError &error);

    void initialize(LowlError &error);

    void terminate(LowlError &error);

    std::unique_ptr<AudioStream>
    create_stream(void *p_buffer, uint32_t p_length, FileFormat p_format, LowlError &error);

    std::unique_ptr<AudioStream> create_stream(const std::string &p_path, LowlError &error);

    std::unique_ptr<AudioReader> create_reader(FileFormat p_format, LowlError &error);

    FileFormat detect_format(const std::string &p_path, LowlError &error);
}
#endif /* LOWL_H */