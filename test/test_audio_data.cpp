#include <doctest/doctest.h>

#include <lowl.h>

#include <iostream>
#include <memory>

TEST_CASE("AudioData") {
    std::vector<Lowl::AudioFrame> audio_frames = std::vector<Lowl::AudioFrame>();
    audio_frames.push_back(Lowl::AudioFrame(0.5, 0.5));
    std::shared_ptr<Lowl::AudioData> audio_data = std::make_unique<Lowl::AudioData>(audio_frames, 44100.0,
                                                                                    Lowl::AudioChannel::Stereo);
    Lowl::AudioFrame read;
    Lowl::AudioSource::ReadResult result;

    SUBCASE("AudioData - Frame") {
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Read);
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Remove);
    }

    SUBCASE("AudioData - Panning") {
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Remove);

        audio_data->set_panning(1);
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, 0.0);
        REQUIRE_EQ(read.right, doctest::Approx(0.70711));
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Remove);

        audio_data->set_panning(-1);
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, doctest::Approx(0.70711));
        REQUIRE_EQ(read.right, 0.0);
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Remove);

        audio_data->set_panning(0);
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        result = audio_data->read(read);
        REQUIRE_EQ(result, Lowl::AudioSource::ReadResult::Remove);
    }
}



