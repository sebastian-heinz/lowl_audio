#ifndef LOWL_AUDIO_DEVICE_H
#define LOWL_AUDIO_DEVICE_H

#include "lowl_error.h"

#include "audio/lowl_audio_buffer.h"
#include "audio/source/lowl_audio_source.h"
#include "audio/backend/lowl_audio_device_properties.h"

#include <vector>

namespace Lowl::Audio {
    class AudioDevice {
    protected:
        struct _constructor_tag {
            explicit _constructor_tag() = default;
        };

        virtual ~AudioDevice() = 0;

        std::shared_ptr<AudioSource> audio_source;
        AudioDeviceProperties audio_device_properties{};
        std::unique_ptr<AudioBuffer> render_buffer;
        std::vector<AudioDeviceProperties> properties_list;
        std::string name;

        void allocate_render_buffer(unsigned long p_frame_capacity);

        void render_to_device_buffer(
            void *p_dst,
            unsigned long p_frames_per_buffer,
            unsigned long p_bytes_per_frame
        );

    public:
        AudioDevice(_constructor_tag);

        void set_name(const std::string &p_name);

        virtual void start(
            AudioDeviceProperties p_audio_device_properties,
            std::shared_ptr<AudioSource> p_audio_source,
            Error &error
        ) = 0;

        virtual void stop(Error &error) = 0;

        std::vector<AudioDeviceProperties> get_properties_list() const;

        AudioDeviceProperties
        get_closest_properties(AudioDeviceProperties p_audio_device_properties, Error &error) const;

        std::string get_name() const;
    };
}

#endif
