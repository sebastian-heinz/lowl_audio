#ifndef LOWL_AUDIO_READER_H
#define LOWL_AUDIO_READER_H

#include <lowl_error.h>

#include <vector>

#include "audio/convert/lowl_audio_sample_converter.h"
#include "audio/lowl_audio_format.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_stream.h"
#include "lowl_file_format.h"

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
        virtual std::unique_ptr<AudioData> read(std::unique_ptr<uint8_t[]> p_buffer, size_t p_length, Error &error) = 0;

        /**
         * check if this file reader can handle provided format.
         */
        virtual bool support(FileFormat p_file_format) const = 0;

        virtual ~AudioReader() = default;

        std::unique_ptr<AudioData> create_audio_data(AudioFormat p_audio_format,
                                                     SampleFormat p_sample_format,
                                                     ChannelLayout p_layout,
                                                     SampleRate p_sample_rate,
                                                     const std::unique_ptr<uint8_t[]> &p_buffer,
                                                     size_t p_size,
                                                     const std::vector<Speaker> &p_input_speakers,
                                                     Error &error);

        std::unique_ptr<AudioData> create_audio_data(ChannelLayout p_layout,
                                                     const std::vector<float> &p_samples,
                                                     SampleRate p_sample_rate,
                                                     const std::vector<Speaker> &p_input_speakers,
                                                     Error &error);

    public:
        AudioReader();

        std::unique_ptr<AudioData> read_file(const std::string &p_path, Error &error);
    };
} // namespace Lowl::Audio

#endif
