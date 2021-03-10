#ifndef LOWL_DEVICE_H
#define LOWL_DEVICE_H

#include "lowl_audio_stream.h"

namespace Lowl {

    class Device {

    private:
        std::string name;

    public:
        virtual void set_stream(std::unique_ptr<AudioStream> p_audio_stream, Error &error) = 0;

        virtual void start(Error &error) = 0;

        virtual void stop(Error &error) = 0;

    public:
        std::string get_name() const;

        void set_name(const std::string &p_name);

        Device() = default;;

        virtual ~Device() = default;;
    };
}

#endif
