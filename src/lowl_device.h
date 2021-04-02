#ifndef LOWL_DEVICE_H
#define LOWL_DEVICE_H

#include "lowl_audio_stream.h"
#include "lowl_audio_mixer.h"

namespace Lowl {

    class Device {

    private:
        std::string name;

    protected:
        std::shared_ptr<AudioStream> audio_stream;
        std::shared_ptr<AudioMixer> audio_mixer;

    public:
        virtual void start_stream(std::shared_ptr<AudioStream> p_audio_stream, Error &error) = 0;

        virtual void start_mixer(std::shared_ptr<AudioMixer> p_audio_mixer, Error &error) = 0;

        virtual void stop(Error &error) = 0;

        virtual bool is_playing() const;

    public:
        std::string get_name() const;

        void set_name(const std::string &p_name);

        Device() = default;

        virtual ~Device() = default;

#ifdef LOWL_PROFILING
    public:
        uint64_t callback_count;
        double callback_total_duration;
        double callback_max_duration;
        double callback_min_duration;
        double callback_avg_duration;
        double time_request_ms;
#endif
    };
}

#endif
