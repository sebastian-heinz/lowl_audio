#include "lowl_audio_stream.h"

Lowl::Audio::AudioStream::AudioStream(SampleRate p_sample_rate, AudioChannel p_channel) : AudioSource(p_sample_rate, p_channel) {
    frame_queue = std::make_unique<moodycamel::ReaderWriterQueue<AudioFrame>>(100);
}

Lowl::Audio::AudioSource::ReadResult Lowl::Audio::AudioStream::read(AudioFrame &audio_frame) {
    if (!is_playing) {
        return ReadResult::Pause;
    }
    if (!frame_queue->try_dequeue(audio_frame)) {
        return ReadResult::End;
    }
    process_volume(audio_frame);
    process_panning(audio_frame);
    return ReadResult::Read;
}

bool Lowl::Audio::AudioStream::write(const AudioFrame &p_audio_frame) {
    if (!frame_queue->enqueue(p_audio_frame)) { // creates copy
        return false;
    }
    return true;
}

void Lowl::Audio::AudioStream::write(const std::vector<AudioFrame> &p_audio_frames) {
    for (const AudioFrame &frame : p_audio_frames) {
        write(frame);
    }
}

Lowl::size_l Lowl::Audio::AudioStream::get_frames_remaining() const {
    return frame_queue->size_approx();
}

Lowl::size_l Lowl::Audio::AudioStream::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioStream::get_frame_count() const {
    return 0;
}
