#include <doctest/doctest.h>

#include <lowl.h>

#include <iostream>
#include <memory>

TEST_CASE("AudioData") {
    std::vector<Lowl::AudioFrame> audio_frames = std::vector<Lowl::AudioFrame>();
    audio_frames.push_back(Lowl::AudioFrame(0.5, 0.5));
    std::shared_ptr<Lowl::AudioData> audio_data = std::make_unique<Lowl::AudioData>(audio_frames, 44100.0,
                                                                                    Lowl::Channel::Stereo);
    Lowl::AudioFrame read;

    SUBCASE("AudioData - Frame") {
        read = Lowl::AudioFrame(1, 1);

        REQUIRE(audio_data->read(read));
        Lowl::AudioFrame previous;
        previous.left = read.left;
        previous.right = read.right;
        REQUIRE_FALSE(audio_data->read(read));

        for (int i = 0; i < 100; i++) {
            REQUIRE(audio_data->read(read));
            REQUIRE_EQ(read.left, previous.left);
            REQUIRE_EQ(read.right, previous.right);
            REQUIRE_FALSE(audio_data->read(read));
            previous.left = read.left;
            previous.right = read.right;
            read.left = 0.8;
        }

        for (int i = 0; i < 100; i++) {
            audio_data->set_panning(1);
            REQUIRE(audio_data->read(read));
            REQUIRE_EQ(read.left, 0.0);
            REQUIRE_EQ(read.right, doctest::Approx(0.70711));
            REQUIRE_FALSE(audio_data->read(read));
        }
    }

    SUBCASE("AudioData - Panning") {
        REQUIRE(audio_data->read(read));
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        REQUIRE_FALSE(audio_data->read(read));

        audio_data->set_panning(1);
        REQUIRE(audio_data->read(read));
        REQUIRE_EQ(read.left, 0.0);
        REQUIRE_EQ(read.right, doctest::Approx(0.70711));
        REQUIRE_FALSE(audio_data->read(read));

        audio_data->set_panning(-1);
        REQUIRE(audio_data->read(read));
        REQUIRE_EQ(read.left, doctest::Approx(0.70711));
        REQUIRE_EQ(read.right, 0.0);
        REQUIRE_FALSE(audio_data->read(read));

        audio_data->set_panning(0);
        REQUIRE(audio_data->read(read));
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        REQUIRE_FALSE(audio_data->read(read));
    }
}



