#ifndef LOWL_AUDIO_DEVICE_H
#define LOWL_AUDIO_DEVICE_H

#include "lowl_error.h"

#include "audio/lowl_audio_source.h"

namespace Lowl::Audio {

    class AudioDevice {

    private:
        std::string name;

    protected:
        std::shared_ptr<AudioSource> audio_source;
        bool exclusive_mode;

    public:
        virtual void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) = 0;

        virtual void stop(Error &error) = 0;

        virtual bool is_supported(Lowl::Audio::AudioChannel channel, Lowl::SampleRate sample_rate, Lowl::Audio::SampleFormat sample_format,
                                  Error &error) = 0;

        virtual Lowl::SampleRate get_default_sample_rate() = 0;

        virtual void set_exclusive_mode(bool p_exclusive_mode, Error &error) = 0;

        bool is_exclusive_mode() const;

        bool is_supported(std::shared_ptr<AudioSource> p_audio_source, Error &error);

        std::string get_name() const;

        void set_name(const std::string &p_name);

        AudioDevice();

        virtual ~AudioDevice() = default;
    };
}

#endif
