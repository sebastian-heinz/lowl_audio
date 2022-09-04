#ifndef LOWL_AUDIO_DEVICE_H
#define LOWL_AUDIO_DEVICE_H

#include "lowl_error.h"

#include "audio/lowl_audio_source.h"

#include <memory>

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

        virtual SampleRate get_default_sample_rate() = 0;

        std::string get_name() const;

        void set_name(const std::string &p_name);

        AudioDevice();

        virtual ~AudioDevice() = default;
    };
}

#endif
