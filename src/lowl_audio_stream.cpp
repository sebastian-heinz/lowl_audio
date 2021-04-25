#include "lowl_audio_stream.h"

Lowl::AudioStream::AudioStream(SampleRate p_sample_rate, Channel p_channel, Volume p_volume, Panning p_panning)
        : AudioSource(p_sample_rate, p_channel, p_volume, p_panning) {
    frame_queue = std::make_unique<moodycamel::ReaderWriterQueue<AudioFrame>>(100);
    frames_in = 0;
    frames_out = 0;
}

bool Lowl::AudioStream::read(AudioFrame &audio_frame) {
    if (!frame_queue->try_dequeue(audio_frame)) {
        return false;
    }
    process_volume(audio_frame);
    process_panning(audio_frame);
    frames_out++;
    return true;
}

bool Lowl::AudioStream::write(const AudioFrame &p_audio_frame) {
    if (!frame_queue->enqueue(p_audio_frame)) {
        return false;
    }
    frames_in++;
    return true;
}

void Lowl::AudioStream::write(const std::vector<AudioFrame> &p_audio_frames) {
    for (const AudioFrame &frame : p_audio_frames) {
        write(frame);
    }
}

uint32_t Lowl::AudioStream::get_num_frame_read() const {
    return frames_out;
}

uint32_t Lowl::AudioStream::get_num_frame_write() const {
    return frames_in;
}

Lowl::size_l Lowl::AudioStream::get_frames_remaining() const {
    return frame_queue->size_approx();
}