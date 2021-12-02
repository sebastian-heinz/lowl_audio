#ifdef LOWL_DRIVER_CORE_AUDIO

#include "lowl_audio_core_audio_device.h"

#include "lowl_logger.h"

#include "audio/coreaudio/lowl_audio_core_audio_utilities.h"

void Lowl::Audio::CoreAudioDevice::start(std::shared_ptr<AudioSource> p_audio_source, Lowl::Error &error) {

}

void Lowl::Audio::CoreAudioDevice::stop(Lowl::Error &error) {

}

bool Lowl::Audio::CoreAudioDevice::is_supported(AudioChannel channel, Lowl::SampleRate sample_rate,
                                                SampleFormat sample_format, Lowl::Error &error) {
    return false;
}

Lowl::SampleRate Lowl::Audio::CoreAudioDevice::get_default_sample_rate() {
    return 0;
}

void Lowl::Audio::CoreAudioDevice::set_exclusive_mode(bool p_exclusive_mode, Lowl::Error &error) {

}

std::unique_ptr<Lowl::Audio::CoreAudioDevice>
Lowl::Audio::CoreAudioDevice::create(const std::string &p_driver_name, AudioObjectID p_device_id, Error &error) {
    LOWL_LOG_DEBUG_F("Device:%d - creating", p_device_id);

    std::string device_name = Lowl::Audio::CoreAudioUtilities::get_device_name(p_device_id, error);
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - name: %s", p_device_id, device_name.c_str());

    uint32_t input_stream_count = Lowl::Audio::CoreAudioUtilities::get_num_stream(
            p_device_id,
            kAudioDevicePropertyScopeInput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - input_stream_count: %d", p_device_id, input_stream_count);

    uint32_t output_stream_count = Lowl::Audio::CoreAudioUtilities::get_num_stream(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - output_stream_count: %d", p_device_id, output_stream_count);
    if (output_stream_count <= 0) {
        // has no output
        error.set_error(ErrorCode::Error);
        return nullptr;
    }

    Lowl::SampleRate default_sample_rate = Lowl::Audio::CoreAudioUtilities::get_device_default_sample_rate(
            p_device_id,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - default_sample_rate: %f", p_device_id, default_sample_rate);

    uint32_t input_channel_count = Lowl::Audio::CoreAudioUtilities::get_num_channel(
            p_device_id,
            kAudioDevicePropertyScopeInput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - input_channel_count: %d", p_device_id, input_channel_count);

    uint32_t output_channel_count = Lowl::Audio::CoreAudioUtilities::get_num_channel(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - output_channel_count: %d", p_device_id, output_channel_count);

    std::vector<AudioObjectID> output_streams = Lowl::Audio::CoreAudioUtilities::get_stream_ids(
            p_device_id,
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - output_streams count: %zu", p_device_id, output_streams.size());

    uint32_t latency_high = Lowl::Audio::CoreAudioUtilities::get_latency_high(
            p_device_id,
            output_streams[0],
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - output_streams[0] latency_high: %d", p_device_id, latency_high);

    uint32_t latency_low = Lowl::Audio::CoreAudioUtilities::get_latency_low(
            64,
            p_device_id,
            output_streams[0],
            kAudioDevicePropertyScopeOutput,
            error
    );
    if (error.has_error()) {
        return nullptr;
    }
    LOWL_LOG_DEBUG_F("Device:%d - output_streams[0] latency_low: %d", p_device_id, latency_low);

    std::unique_ptr<CoreAudioDevice> device = std::unique_ptr<CoreAudioDevice>(new CoreAudioDevice());
    device->set_name("[" + p_driver_name + "] " + device_name);
    device->device_id = p_device_id;
    device->input_stream_count = input_stream_count;
    device->output_stream_count = output_stream_count;
    LOWL_LOG_DEBUG_F("Device:%d - created", p_device_id);

    return device;
}

Lowl::Audio::CoreAudioDevice::CoreAudioDevice() : AudioDevice() {

}

Lowl::Audio::CoreAudioDevice::~CoreAudioDevice() {

}

#endif /* LOWL_DRIVER_CORE_AUDIO */