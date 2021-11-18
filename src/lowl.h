#ifndef LOWL_H
#define LOWL_H

#include "lowl_logger.h"
#include "lowl_file_format.h"

#include "audio/lowl_audio_driver.h"
#include "audio/lowl_audio_device.h"
#include "audio/lowl_audio_data.h"
#include "audio/reader/lowl_audio_reader.h"

#include <vector>

namespace Lowl {

    class Lib {

    private:
        static std::atomic_flag initialized;
        static std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> drivers;

    public:

        static std::vector<std::shared_ptr<Lowl::Audio::AudioDriver>> get_drivers(Lowl::Error &error);

        static void initialize(Lowl::Error &error);

        static void terminate(Lowl::Error &error);

        static std::unique_ptr<Lowl::Audio::AudioData>
        create_data(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, Lowl::FileFormat p_format, Lowl::Error &error);

        static std::unique_ptr<Lowl::Audio::AudioData> create_data(const std::string &p_path, Lowl::Error &error);

        static std::unique_ptr<Lowl::Audio::AudioReader> create_reader(Lowl::FileFormat p_format, Lowl::Error &error);

        static Lowl::FileFormat detect_format(const std::string &p_path, Lowl::Error &error);

        static std::shared_ptr<Lowl::Audio::AudioDevice> get_default_device(Lowl::Error &error);
    };
}
#endif /* LOWL_H */