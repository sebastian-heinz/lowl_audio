#ifndef LOWL_AUDIO_DEVICE_H
#define LOWL_AUDIO_DEVICE_H

#include "lowl_error.h"

#include "audio/lowl_audio_source.h"
#include "audio/lowl_audio_device_properties.h"

#include <memory>
#include <vector>

namespace Lowl::Audio {

    class AudioDevice {

    private:
        std::string name;

    protected:
        std::shared_ptr<AudioSource> audio_source;
        std::vector<AudioDeviceProperties> properties;

    public:
        virtual void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) = 0;

        virtual void stop(Error &error) = 0;

        std::vector<AudioDeviceProperties> get_properties() const;

        AudioDeviceProperties get_closest_properties(AudioDeviceProperties p_audio_device_properties, Error &error) const;

        std::string get_name() const;

        void set_name(const std::string &p_name);

        AudioDevice();

        virtual ~AudioDevice() = default;
    };
}

#endif
