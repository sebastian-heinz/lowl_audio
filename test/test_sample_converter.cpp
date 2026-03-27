#include <doctest/doctest.h>

#include "audio/convert/lowl_audio_sample_converter.h"

#include <limits>

TEST_CASE("SampleConverter") {
    SUBCASE("SampleConverter - integer conversions clamp out-of-range samples") {
        REQUIRE_EQ(Lowl::Audio::SampleConverter::sample_to_int16(2.0f), 32767);
        REQUIRE_EQ(Lowl::Audio::SampleConverter::sample_to_int16(-2.0f), -32767);
        REQUIRE_EQ(Lowl::Audio::SampleConverter::sample_to_int8(2.0f), 127);
        REQUIRE_EQ(Lowl::Audio::SampleConverter::sample_to_int8(-2.0f), -127);
        REQUIRE_EQ(
            Lowl::Audio::SampleConverter::sample_to_int32(2.0f),
            std::numeric_limits<int32_t>::max()
        );
        REQUIRE_EQ(
            Lowl::Audio::SampleConverter::sample_to_int32(-2.0f),
            -std::numeric_limits<int32_t>::max()
        );
    }
}
