#include "lowl_audio_stream.h"

Lowl::AudioStream::AudioStream(SampleRate p_sample_rate, Channel p_channel) : AudioSource(p_sample_rate, p_channel) {
    frame_queue = std::make_unique<moodycamel::ReaderWriterQueue<AudioFrame>>(100);
}

bool Lowl::AudioStream::read(AudioFrame &audio_frame) {
    if (!frame_queue->try_dequeue(audio_frame)) {
        return false;
    }
    process_volume(audio_frame);
    process_panning(audio_frame);
    return true;
}

bool Lowl::AudioStream::write(const AudioFrame &p_audio_frame) {
    if (!frame_queue->enqueue(p_audio_frame)) {
        return false;
    }
    return true;
}

void Lowl::AudioStream::write(const std::vector<AudioFrame> &p_audio_frames) {
    for (const AudioFrame &frame : p_audio_frames) {
        write(frame);
    }
}

Lowl::size_l Lowl::AudioStream::get_frames_remaining() const {
    return frame_queue->size_approx();
}