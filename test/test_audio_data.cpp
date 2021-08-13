#include <doctest/doctest.h>

#include <lowl.h>

#include <iostream>
#include <memory>

TEST_CASE("AudioData") {
    std::vector<Lowl::AudioFrame> audio_frames = std::vector<Lowl::AudioFrame>();
    audio_frames.push_back(Lowl::AudioFrame(0.5,0.5));
    std::shared_ptr<Lowl::AudioData> audio_data = std::make_unique<Lowl::AudioData>(audio_frames, 44100.0, Lowl::Channel::Stereo);
    Lowl::AudioFrame read;
    audio_data->read(read);
    DOCTEST_REQUIRE_EQ(read.left, 0.5);
    DOCTEST_REQUIRE_EQ(read.right, 0.5);
}



