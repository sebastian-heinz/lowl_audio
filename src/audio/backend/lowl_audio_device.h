#ifndef LOWL_AUDIO_DEVICE_H
#define LOWL_AUDIO_DEVICE_H
	
#include <memory>
#include <vector>
	
#include "audio/backend/lowl_audio_device_properties.h"
#include "audio/lowl_audio_buffer.h"
#include "audio/source/lowl_audio_source.h"
#include "lowl_error.h"

namespace Lowl::Audio {
    class AudioDevice {
    protected:
        struct _constructor_tag {
            explicit _constructor_tag() = default;
        };

        struct RenderState {
            AudioDeviceProperties audio_device_properties{};
            std::shared_ptr<AudioSource> audio_source;
            AudioBuffer render_buffer{};
        };

        virtual ~AudioDevice() = 0;

        std::shared_ptr<AudioSource> audio_source;
        AudioDeviceProperties audio_device_properties{};
        std::shared_ptr<RenderState> render_state;
        std::vector<AudioDeviceProperties> properties_list;
        std::string name;

        void allocate_render_buffer(unsigned long p_frame_capacity);

        std::shared_ptr<RenderState> load_render_state() const;

        void clear_render_state();

        void render_to_device_buffer(const std::shared_ptr<RenderState> &p_render_state,
                                     void *p_dst,
                                     size_t p_dst_byte_size,
                                     unsigned long p_frames_per_buffer,
                                     unsigned long p_bytes_per_frame);

    public:
        AudioDevice(_constructor_tag);

        void set_name(const std::string &p_name);

        virtual void start(AudioDeviceProperties p_audio_device_properties,
                           std::shared_ptr<AudioSource> p_audio_source,
                           Error &error) = 0;

        virtual void stop(Error &error) = 0;

        std::vector<AudioDeviceProperties> get_properties_list() const;

        AudioDeviceProperties get_closest_properties(AudioDeviceProperties p_audio_device_properties,
                                                     Error &error) const;

        std::string get_name() const;
    };
} // namespace Lowl::Audio

#endif
