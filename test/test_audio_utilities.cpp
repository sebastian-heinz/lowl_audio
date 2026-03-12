#include <doctest/doctest.h>

#include "audio/lowl_audio_utilities.h"

#include <memory>
#include <vector>

TEST_CASE("AudioUtilities") {
    SUBCASE("AudioUtilities - to_stream preserves more than 100 frames") {
        std::vector<Lowl::Audio::AudioFrame> frames;
        for (size_t frame_index = 0; frame_index < 150; frame_index++) {
            Lowl::Sample sample = static_cast<Lowl::Sample>(frame_index) / 200.0f;
            frames.emplace_back(sample, -sample);
        }

        std::shared_ptr<Lowl::Audio::AudioData> audio_data = std::make_unique<Lowl::Audio::AudioData>(
            frames,
            44100.0,
            Lowl::Audio::AudioChannel::Stereo
        );

        Lowl::Error error;
        std::unique_ptr<Lowl::Audio::AudioStream> stream = Lowl::Audio::Utilities::to_stream(audio_data, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(stream != nullptr);

        Lowl::Audio::AudioFrame read;
        for (const Lowl::Audio::AudioFrame &expected_frame : frames) {
            REQUIRE_EQ(stream->read(read), Lowl::Audio::AudioSource::ReadResult::Read);
            REQUIRE_EQ(read.left, doctest::Approx(expected_frame.left));
            REQUIRE_EQ(read.right, doctest::Approx(expected_frame.right));
        }

        REQUIRE_EQ(stream->read(read), Lowl::Audio::AudioSource::ReadResult::End);
    }
}
