#include <doctest/doctest.h>

#include "audio/convert/lowl_audio_sample_converter.h"
#include "audio/lowl_audio_sample_format.h"

#include <array>
#include <cstring>
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

    SUBCASE("SampleConverter - int24 conversion preserves signed values") {
        REQUIRE_EQ(Lowl::Audio::SampleConverter::sample_to_int24(1.0f), 0x7FFFFF);
        REQUIRE_EQ(Lowl::Audio::SampleConverter::sample_to_int24(-1.0f), -0x7FFFFF);
        REQUIRE_LT(Lowl::Audio::SampleConverter::sample_to_int24(-0.5f), 0);
    }

    SUBCASE("SampleConverter - write_sample writes FLOAT_64 output and advances destination") {
        std::array<std::byte, sizeof(double)> storage{};
        void *write_ptr = storage.data();

        const bool wrote = Lowl::Audio::SampleConverter::write_sample(
            Lowl::Audio::SampleFormat::FLOAT_64,
            static_cast<Lowl::Sample>(0.25),
            &write_ptr
        );

        REQUIRE(wrote);
        REQUIRE_EQ(static_cast<std::byte *>(write_ptr), storage.data() + sizeof(double));

        double value = 0.0;
        std::memcpy(&value, storage.data(), sizeof(value));
        REQUIRE_EQ(value, doctest::Approx(0.25));
    }

    SUBCASE("SampleConverter - write_sample rejects Unknown format without advancing destination") {
        std::array<std::byte, sizeof(float)> storage{};
        void *write_ptr = storage.data();

        const bool wrote = Lowl::Audio::SampleConverter::write_sample(
            Lowl::Audio::SampleFormat::Unknown,
            static_cast<Lowl::Sample>(0.25),
            &write_ptr
        );

        REQUIRE_FALSE(wrote);
        REQUIRE_EQ(write_ptr, storage.data());
    }
}
