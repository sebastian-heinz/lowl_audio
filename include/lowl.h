#ifndef LOWL_H
#define LOWL_H

#include <vector>

#include "lowl_driver.h"
#include "lowl_file_format.h"
#include "lowl_audio_reader.h"
#include "lowl_audio_stream.h"

class Lowl {
private:
    static std::vector<LowlDriver *> drivers;

public:
    static std::vector<LowlDriver *> get_drivers(LowlError &error);

    static void initialize(LowlError &error);

    static void terminate(LowlError &error);

    static std::unique_ptr<LowlAudioStream>
    create_stream(void *p_buffer, uint32_t p_length, LowlFileFormat format, LowlError &error);

    static std::unique_ptr<LowlAudioStream> create_stream(const std::string &p_path, LowlError &error);

    static std::unique_ptr<LowlAudioReader> create_reader(LowlFileFormat format, LowlError &error);

    static LowlFileFormat detect_format(const std::string &p_path, LowlError &error);
};

#endif /* LOWL_H */