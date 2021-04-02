#include "lowl_device.h"

std::string Lowl::Device::get_name() const {
    return name;
}

void Lowl::Device::set_name(const std::string &p_name) {
    name = p_name;
}

bool Lowl::Device::is_playing() const {
    // TODO if stream temporary starves this would be wrong
    if (!audio_stream) {
        return false;
    }
    return audio_stream->get_num_frame_read() < audio_stream->get_num_frame_write();
}