#ifndef LOWL_H
#define LOWL_H

#include "../src/lowl_audio_reader.h"
#include "../src/lowl_audio_stream.h"
#include "../src/lowl_driver.h"
#include "../src/lowl_file_format.h"

#include <vector>

namespace Lowl {

    class Lib {

    public:
        static std::vector<Driver *> get_drivers(Error &error);

        static void initialize(Error &error);

        static void terminate(Error &error);

        static std::unique_ptr<AudioStream>
        create_stream(void *p_buffer, uint32_t p_length, FileFormat p_format, Error &error);

        static std::unique_ptr<AudioStream> create_stream(const std::string &p_path, Error &error);

        static std::unique_ptr<AudioReader> create_reader(FileFormat p_format, Error &error);

        static FileFormat detect_format(const std::string &p_path, Error &error);
    };


}
#endif /* LOWL_H */