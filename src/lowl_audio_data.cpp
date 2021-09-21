#include "lowl_audio_data.h"

#include <algorithm>

Lowl::AudioData::AudioData(std::vector<Lowl::AudioFrame> p_audio_frames, SampleRate p_sample_rate, Channel p_channel)
        : AudioSource(p_sample_rate, p_channel) {
    frames = std::vector<AudioFrame>(p_audio_frames);
    position = 0;
    seek_position = 0;
    size = frames.size();
    is_not_reset.test_and_set();
}

Lowl::AudioSource::ReadResult Lowl::AudioData::read(Lowl::AudioFrame &audio_frame) {
    if (!is_playing) {
        return ReadResult::Pause;
    }
    if (!is_not_reset.test_and_set()) {
        position = seek_position.load();
        seek_position = 0;
    }
    if (position >= size) {
        position = 0;
        return ReadResult::Remove;
    }
    audio_frame = frames[position]; // creates copy
    process_volume(audio_frame);
    process_panning(audio_frame);
    position++;
    return ReadResult::Read;
}

std::vector<Lowl::AudioFrame> Lowl::AudioData::get_frames() {
    return std::vector<AudioFrame>(frames);
}

std::unique_ptr<Lowl::AudioData> Lowl::AudioData::create_slice(TimeSeconds p_begin_sec, TimeSeconds p_end_sec) {
    size_t first_frame = static_cast<size_t>(p_begin_sec * sample_rate);
    size_t last_frame = static_cast<size_t>(p_end_sec * sample_rate);
    std::clamp<size_t>(first_frame, 0, size - 1);
    std::clamp<size_t>(last_frame, 0, size - 1);
    std::vector<AudioFrame> slice;
    if (p_end_sec > 0.0) {
        slice = std::vector<AudioFrame>(
                frames.begin() + static_cast<std::vector<AudioFrame>::difference_type>(first_frame),
                frames.begin() + static_cast<std::vector<AudioFrame>::difference_type>(last_frame)
        );
    } else {
        slice = std::vector<AudioFrame>(
                frames.begin() + static_cast<std::vector<AudioFrame>::difference_type>(first_frame),
                frames.end()
        );
    }
    return std::make_unique<Lowl::AudioData>(slice, sample_rate, channel);
}

Lowl::size_l Lowl::AudioData::get_frames_remaining() const {
    int64_t remaining = static_cast<int64_t>(size - position);
    if (remaining < 0) {
        remaining = 0;
    }
    return static_cast<size_l>(remaining);
}

Lowl::AudioData::~AudioData() {
}

void Lowl::AudioData::reset() {
    seek_position = 0;
    is_not_reset.clear();
}

void Lowl::AudioData::seek_frame(size_t p_frame) {
    std::clamp<size_t>(p_frame, 0, size - 1);
    seek_position = p_frame;
    is_not_reset.clear();
}

void Lowl::AudioData::seek_time(Lowl::double_l p_seconds) {
    size_t frame = p_seconds * sample_rate;
    std::clamp<size_t>(frame, 0, size - 1);
    seek_position = frame;
    is_not_reset.clear();
}

Lowl::size_l Lowl::AudioData::get_frame_position() const {
    return position;
}
