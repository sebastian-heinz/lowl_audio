#ifndef LOWL_DEVICE_H
#define LOWL_DEVICE_H

#include "lowl_audio_stream.h"

namespace Lowl {

    class Device {

    private:
        std::string name;

    protected:
        std::shared_ptr<AudioStream> audio_stream;

    public:

        virtual void start(Error &error) = 0;

        virtual void stop(Error &error) = 0;

        virtual bool is_playing() const;

        virtual void set_stream(std::shared_ptr<AudioStream> p_audio_stream, Error &error);

    public:
        std::string get_name() const;

        void set_name(const std::string &p_name);

        Device() = default;

        virtual ~Device() = default;
    };
}

#endif
