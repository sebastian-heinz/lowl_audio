#include <doctest/doctest.h>

#include <lowl.h>

#include <iostream>
#include <memory>

TEST_CASE("AudioStream") {
    std::shared_ptr<Lowl::Audio::AudioStream> audio_stream
            = std::make_unique<Lowl::Audio::AudioStream>(44100.0, Lowl::Audio::AudioChannel::Stereo);

    Lowl::Audio::AudioFrame read;
    Lowl::Audio::AudioSource::ReadResult result;

    SUBCASE("AudioStream - Frame") {
        REQUIRE(audio_stream->write(Lowl::Audio::AudioFrame(0.5, 0.5)));
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::Read);
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::End);
    }

    SUBCASE("AudioStream - Panning") {
        REQUIRE(audio_stream->write(Lowl::Audio::AudioFrame(0.5, 0.5)));
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::End);

        audio_stream->set_panning(1);
        REQUIRE(audio_stream->write(Lowl::Audio::AudioFrame(0.5, 0.5)));
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, 0.0);
        REQUIRE_EQ(read.right, doctest::Approx(0.70711));
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::End);

        audio_stream->set_panning(-1);
        REQUIRE(audio_stream->write(Lowl::Audio::AudioFrame(0.5, 0.5)));
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, doctest::Approx(0.70711));
        REQUIRE_EQ(read.right, 0.0);
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::End);

        audio_stream->set_panning(0);
        REQUIRE(audio_stream->write(Lowl::Audio::AudioFrame(0.5, 0.5)));
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::Read);
        REQUIRE_EQ(read.left, 0.5);
        REQUIRE_EQ(read.right, 0.5);
        result = audio_stream->read(read);
        REQUIRE_EQ(result, Lowl::Audio::AudioSource::ReadResult::End);
    }
}



