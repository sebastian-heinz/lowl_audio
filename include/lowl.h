#ifndef LOWL_H
#define LOWL_H

#include "../src/lowl_audio_reader.h"
#include "../src/lowl_audio_data.h"
#include "../src/lowl_audio_stream.h"
#include "../src/lowl_audio_driver.h"
#include "../src/lowl_file_format.h"
#include "../src/lowl_audio_mixer.h"
#include "../src/lowl_audio_space.h"
#include "../src/lowl_audio_util.h"
#include "../src/lowl_logger.h"

#include <vector>

namespace Lowl {

    class Lib {

    private:
        static std::atomic_flag initialized;
        static std::vector<std::shared_ptr<Lowl::AudioDriver>> drivers;

    public:

        static std::vector<std::shared_ptr<Lowl::AudioDriver>> get_drivers(Error &error);

        static void initialize(Error &error);

        static void terminate(Error &error);

        static std::unique_ptr<AudioData>
        create_data(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, FileFormat p_format, Error &error);

        static std::unique_ptr<AudioData> create_data(const std::string &p_path, Error &error);

        static std::unique_ptr<AudioReader> create_reader(FileFormat p_format, Error &error);

        static FileFormat detect_format(const std::string &p_path, Error &error);

        static std::shared_ptr<AudioDevice> get_default_device(Error &error);
    };
}
#endif /* LOWL_H */