#ifndef LOWL_DEVICE_H
#define LOWL_DEVICE_H

#include "lowl_audio_stream.h"
#include "lowl_audio_mixer.h"

namespace Lowl {

    class Device {

    private:
        std::string name;

    protected:
        std::shared_ptr<AudioSource> audio_source;
        bool exclusive_mode;

    public:
        virtual void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) = 0;

        virtual void stop(Error &error) = 0;

        virtual bool is_supported(Lowl::Channel channel, Lowl::SampleRate sample_rate, Lowl::SampleFormat sample_format,
                                  Error &error) = 0;

        virtual Lowl::SampleRate get_default_sample_rate() = 0;

        virtual void set_exclusive_mode(bool p_exclusive_mode, Error &error) = 0;

        bool is_exclusive_mode() const;

        bool is_supported(std::shared_ptr<AudioSource> p_audio_source, Error &error);

        std::string get_name() const;

        void set_name(const std::string &p_name);

        Device();

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
