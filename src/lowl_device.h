#ifndef LOWL_DEVICE_H
#define LOWL_DEVICE_H

#include <string>

#include "lowl_error.h"
#include "lowl_audio_stream.h"

namespace Lowl {

    class Device {

    private:
        std::string name;

    public:
        virtual void set_stream(std::unique_ptr<LowlAudioStream> p_audio_stream, LowlError &error) = 0;

        virtual void start(LowlError &error) = 0;

        virtual void stop(LowlError &error) = 0;

    public:
        std::string get_name() const;

        void set_name(const std::string &p_name);

        Device() = default;;

        virtual ~Device() = default;;
    };
}

#endif
