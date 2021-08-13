#include <doctest/doctest.h>

#include <lowl.h>

#include <iostream>
#include <memory>

TEST_CASE("AudioStream") {
    std::shared_ptr<Lowl::AudioStream> audio_stream
            = std::make_unique<Lowl::AudioStream>(44100.0, Lowl::Channel::Stereo);

    Lowl::AudioFrame read;

    SUBCASE("AudioStream - Frame") {
        REQUIRE(audio_stream->write(Lowl::AudioFrame(0.5, 0.5)));
        REQUIRE(audio_stream->read(read));
        Lowl::AudioFrame previous;
        previous.left = read.left;
        previous.right = read.right;
        REQUIRE_FALSE(audio_stream->read(read));

        for (int i = 0; i < 5; i++) {
            REQUIRE(audio_stream->write(Lowl::AudioFrame(0.5, 0.5)));
            REQUIRE(audio_stream->read(read));
            REQUIRE_EQ(read.left, previous.left);
            REQUIRE_EQ(read.right, previous.right);
            REQUIRE_FALSE(audio_stream->read(read));
            previous.left = read.left;
            previous.right = read.right;
            read.left = 0.8;
        }

        for (int i = 0; i < 5; i++) {
            audio_stream->set_panning(1);
            REQUIRE(audio_stream->write(Lowl::AudioFrame(0.5, 0.5)));
            REQUIRE(audio_stream->read(read));
            REQUIRE_EQ(read.left, 0.0);
            REQUIRE_EQ(read.right, doctest::Approx(0.70711));
            REQUIRE_FALSE(audio_stream->read(read));
        }
    }

    SUBCASE("AudioStream - Panning") {
        REQUIRE(audio_stream->write(Lowl::AudioFrame(0.5, 0.5)));
        REQUIRE(audio_stream->read(read));
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        REQUIRE_FALSE(audio_stream->read(read));

        audio_stream->set_panning(1);
        REQUIRE(audio_stream->write(Lowl::AudioFrame(0.5, 0.5)));
        REQUIRE(audio_stream->read(read));
        REQUIRE_EQ(read.left, 0.0);
        REQUIRE_EQ(read.right, doctest::Approx(0.70711));
        REQUIRE_FALSE(audio_stream->read(read));

        audio_stream->set_panning(-1);
        REQUIRE(audio_stream->write(Lowl::AudioFrame(0.5, 0.5)));
        REQUIRE(audio_stream->read(read));
        REQUIRE_EQ(read.left, doctest::Approx(0.70711));
        REQUIRE_EQ(read.right, 0.0);
        REQUIRE_FALSE(audio_stream->read(read));

        audio_stream->set_panning(0);
        REQUIRE(audio_stream->write(Lowl::AudioFrame(0.5, 0.5)));
        REQUIRE(audio_stream->read(read));
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        REQUIRE_FALSE(audio_stream->read(read));
    }
}



