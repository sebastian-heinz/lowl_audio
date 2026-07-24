#include <doctest/doctest.h>

#include "audio/convert/lowl_audio_sample_converter.h"
#include "audio/lowl_audio_sample_format.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>

TEST_CASE("SampleConverter") {
    SUBCASE("SampleConverter - int32 endpoints normalize exactly") {
        REQUIRE_EQ(
            Lowl::Audio::SampleConverter::int32_to_float(std::numeric_limits<int32_t>::max()),
            doctest::Approx(1.0f)
        );
        REQUIRE_EQ(
            Lowl::Audio::SampleConverter::int32_to_float(std::numeric_limits<int32_t>::min()),
            doctest::Approx(-1.0f)
        );
        REQUIRE_EQ(Lowl::Audio::SampleConverter::int32_to_float(0), doctest::Approx(0.0f));
    }

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

#if defined(LOWL_TYPE_SAMPLE_64)
    SUBCASE("SampleConverter - integer output does not narrow double Sample before quantizing") {
        constexpr int32_t target_value = 12345;
        const double threshold = static_cast<double>(target_value) / 32767.0;
        const Lowl::Sample sample = std::nextafter(threshold, 0.0);

        REQUIRE_EQ(Lowl::Audio::SampleConverter::sample_to_int16(sample), target_value - 1);
    }
#endif
}
