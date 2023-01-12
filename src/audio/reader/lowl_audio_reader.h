#ifndef LOWL_AUDIO_READER_H
#define LOWL_AUDIO_READER_H

#include "lowl_file_format.h"

#include "audio/lowl_audio_stream.h"
#include "audio/lowl_audio_format.h"
#include "audio/lowl_audio_data.h"

#include "audio/convert/lowl_audio_sample_converter.h"

#include <vector>

namespace Lowl::Audio {
    class AudioReader {

    public:
        static std::unique_ptr<AudioReader> create_reader(FileFormat p_format, Error &error);

        static FileFormat detect_format(const std::string &p_path, Error &error);

        static std::unique_ptr<Lowl::Audio::AudioData> create_data(const std::string &p_path, Error &error);

        static std::unique_ptr<AudioData>
        create_data(std::unique_ptr<uint8_t[]> p_buffer, size_t p_size, FileFormat p_format, Error &error);

    public:
        /**
         * read data as supported file format.
         */
        virtual std::unique_ptr<AudioData>
        read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_length, Error &error) = 0;

        /**
         * check if this file reader can handle provided format.
         */
        virtual bool support(FileFormat p_file_format) const = 0;

        virtual ~AudioReader() = default;

        virtual std::vector<AudioFrame>
        read_frames(AudioFormat p_audio_format, SampleFormat p_sample_format, AudioChannel p_channel,
                    const std::unique_ptr<uint8_t[]> &p_buffer, size_t p_size, Error &error);

        virtual std::vector<AudioFrame>
        read_frames(AudioChannel p_channel, std::vector<float> samples, Lowl::Error &error);

    public:
        AudioReader();

        std::unique_ptr<AudioData> read_file(const std::string &p_path, Error &error);
    };
}

#endif
