#include "lowl_device.h"

std::string Lowl::Device::get_name() const {
    return name;
}

void Lowl::Device::set_name(const std::string &p_name) {
    name = p_name;
}

void Lowl::Device::set_stream(std::unique_ptr<AudioStream> p_audio_stream, Lowl::Error &error) {
    audio_stream = std::move(p_audio_stream);
}

bool Lowl::Device::is_playing() const {
    if (!audio_stream) {
        return false;
    }
    return audio_stream->get_frames_out() < audio_stream->get_frames_in();
}

uint32_t Lowl::Device::frames_played() const {
    return audio_stream->get_frames_out();
}
