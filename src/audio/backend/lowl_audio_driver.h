#ifndef LOWL_AUDIO_DRIVER_H
#define LOWL_AUDIO_DRIVER_H

#include <vector>

#include "audio/backend/lowl_audio_device.h"

namespace Lowl::Audio {
    class AudioDriver {

    protected:
        std::vector<std::shared_ptr<AudioDevice>> devices;
        std::shared_ptr<AudioDevice> default_device;
        std::string name;

    public:
        virtual void initialize(Error &error) = 0;

        virtual std::shared_ptr<AudioDevice> get_default_device() const;

        const std::vector<std::shared_ptr<AudioDevice>> &get_devices() const;

        std::string get_name() const;

        AudioDriver();

        virtual ~AudioDriver() = default;
        ;
    };
} // namespace Lowl::Audio

#endif
