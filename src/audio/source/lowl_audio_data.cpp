#include "lowl_audio_data.h"

#include <algorithm>
#include <cstring>
#include <utility>

void Lowl::Audio::AudioData::rebuild_channel_ptrs() {
    channel_ptrs.assign(get_channel_num(), nullptr);
    if (!storage) {
        return;
    }
    for (size_t channel_index = 0; channel_index < channel_ptrs.size(); channel_index++) {
        channel_ptrs[channel_index] = storage.get() + channel_index * frame_count;
    }
}

Lowl::Audio::AudioData::AudioData(std::unique_ptr<Sample[]> p_storage,
                                  const size_t p_frame_count,
                                  SampleRate p_sample_rate,
                                  AudioChannel p_channel)
    : storage(std::move(p_storage)), sample_rate(p_sample_rate), channel(p_channel), frame_count(p_frame_count) {
    rebuild_channel_ptrs();
    name = std::string();
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioData::create_slice(TimeSeconds p_begin_sec,
                                                                             TimeSeconds p_end_sec) {
    size_t first_frame = static_cast<size_t>(p_begin_sec * sample_rate);
    size_t last_frame = static_cast<size_t>(p_end_sec * sample_rate);
    if (frame_count == 0) {
        return std::make_unique<AudioData>(std::unique_ptr<Sample[]>(), 0, sample_rate, channel);
    }
    first_frame = std::min(first_frame, frame_count);
    last_frame = p_end_sec > 0.0 ? std::min(last_frame, frame_count) : frame_count;
    if (last_frame < first_frame) {
        last_frame = first_frame;
    }
    const size_t slice_frame_count = last_frame - first_frame;
    std::unique_ptr<Sample[]> slice_storage;
    const size_t channel_count = get_channel_num();
    if (slice_frame_count > 0 && channel_count > 0) {
        slice_storage = std::make_unique<Sample[]>(slice_frame_count * channel_count);
        for (size_t channel_index = 0; channel_index < channel_count; channel_index++) {
            const Sample *src_channel = get_channel_data(static_cast<uint8_t>(channel_index));
            Sample *dst_channel = slice_storage.get() + channel_index * slice_frame_count;
            if (src_channel != nullptr) {
                std::copy_n(src_channel + first_frame, slice_frame_count, dst_channel);
            } else {
                std::memset(dst_channel, 0, slice_frame_count * sizeof(Sample));
            }
        }
    }
    std::unique_ptr<AudioData> audio_data =
        std::make_unique<AudioData>(std::move(slice_storage), slice_frame_count, sample_rate, channel);
    audio_data->set_name(name);
    return audio_data;
}

const Lowl::Sample *Lowl::Audio::AudioData::get_channel_data(const uint8_t p_channel) const {
    if (p_channel >= channel_ptrs.size()) {
        return nullptr;
    }
    return channel_ptrs[static_cast<size_t>(p_channel)];
}

Lowl::Audio::AudioData::~AudioData() {
}

Lowl::SampleRate Lowl::Audio::AudioData::get_sample_rate() const {
    return sample_rate;
}

Lowl::Audio::AudioChannel Lowl::Audio::AudioData::get_channel() const {
    return channel;
}

size_t Lowl::Audio::AudioData::get_channel_num() const {
    return Audio::get_channel_num(channel);
}

Lowl::size_l Lowl::Audio::AudioData::get_frames_remaining() const {
    return frame_count;
}

Lowl::size_l Lowl::Audio::AudioData::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioData::get_frame_count() const {
    return frame_count;
}

std::string Lowl::Audio::AudioData::get_name() const {
    return name;
}

void Lowl::Audio::AudioData::set_name(const std::string &p_name) {
    name = p_name;
}
