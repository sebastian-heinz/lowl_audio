#include "lowl_audio_data.h"

#include <algorithm>
#include <cstring>
#include <utility>

void Lowl::Audio::AudioData::rebuild_channel_ptrs() {
    channel_ptrs.assign(get_channel_count(), nullptr);
    if (!storage) {
        return;
    }
    for (size_t channel_index = 0; channel_index < channel_ptrs.size(); channel_index++) {
        channel_ptrs[channel_index] = storage.get() + channel_index * frame_stride;
    }
}

Lowl::Audio::AudioData::AudioData(std::unique_ptr<Sample[]> p_storage,
                                  const size_t p_frame_count,
                                  const AudioFormat p_audio_format)
    : audio_format(p_audio_format),
      frame_count(p_frame_count) {
    const uint8_t channel_count = get_channel_count();
    frame_stride = aligned_frame_stride(frame_count);
    if (frame_stride > 0 && channel_count > 0) {
        storage = allocate_aligned_samples(frame_stride * channel_count);
        if (p_storage) {
            for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                Sample *dst_channel = storage.get() + static_cast<size_t>(channel_index) * frame_stride;
                const Sample *src_channel = p_storage.get() + static_cast<size_t>(channel_index) * frame_count;
                std::copy_n(src_channel, frame_count, dst_channel);
            }
        }
    }
    rebuild_channel_ptrs();
    name = std::string();
}

std::unique_ptr<Lowl::Audio::AudioData> Lowl::Audio::AudioData::create_slice(TimeSeconds p_begin_sec,
                                                                             TimeSeconds p_end_sec) {
    const TimeSeconds begin_sec = std::max<TimeSeconds>(0.0, p_begin_sec);
    const TimeSeconds end_sec = std::max<TimeSeconds>(0.0, p_end_sec);
    size_t first_frame = static_cast<size_t>(begin_sec * audio_format.sample_rate);
    size_t last_frame = static_cast<size_t>(end_sec * audio_format.sample_rate);
    if (frame_count == 0) {
        return std::make_unique<AudioData>(std::unique_ptr<Sample[]>(), 0, audio_format);
    }
    first_frame = std::min(first_frame, frame_count);
    last_frame = p_end_sec > 0.0 ? std::min(last_frame, frame_count) : frame_count;
    if (last_frame < first_frame) {
        last_frame = first_frame;
    }
    const size_t slice_frame_count = last_frame - first_frame;
    std::unique_ptr<Sample[]> slice_storage;
    const uint8_t channel_count = get_channel_count();
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
        std::make_unique<AudioData>(std::move(slice_storage), slice_frame_count, audio_format);
    audio_data->set_name(get_name());
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

const Lowl::Audio::AudioFormat &Lowl::Audio::AudioData::get_audio_format() const {
    return audio_format;
}

uint8_t Lowl::Audio::AudioData::get_channel_count() const {
    return audio_format.channel_layout.channel_count;
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
    std::lock_guard<std::mutex> lock(name_mutex);
    return name;
}

void Lowl::Audio::AudioData::set_name(const std::string &p_name) {
    std::lock_guard<std::mutex> lock(name_mutex);
    name = p_name;
}
