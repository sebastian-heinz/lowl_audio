#include <doctest/doctest.h>

#include "audio/lowl_audio_channel.h"

TEST_CASE("ChannelLayout") {
    using Lowl::Audio::ChannelLayout;
    using Lowl::Audio::Speaker;

    SUBCASE("from_count returns expected common layouts") {
        REQUIRE_FALSE(ChannelLayout::from_count(0).is_valid());
        REQUIRE_EQ(ChannelLayout::from_count(1), ChannelLayout::Mono);
        REQUIRE_EQ(ChannelLayout::from_count(2), ChannelLayout::Stereo);
        REQUIRE_EQ(ChannelLayout::from_count(3), ChannelLayout::Surround_3_0);
        REQUIRE_EQ(ChannelLayout::from_count(4), ChannelLayout::Quad);
        REQUIRE_EQ(ChannelLayout::from_count(5), ChannelLayout::Surround_5_0);
        REQUIRE_EQ(ChannelLayout::from_count(6), ChannelLayout::Surround_5_1);
        REQUIRE_EQ(ChannelLayout::from_count(7), ChannelLayout::Surround_6_1);
        REQUIRE_EQ(ChannelLayout::from_count(8), ChannelLayout::Surround_7_1);
        REQUIRE_FALSE(ChannelLayout::from_count(9).is_valid());
    }

    SUBCASE("from_mask and speaker queries follow canonical bit order") {
        const ChannelLayout stereo =
            ChannelLayout::from_mask(Lowl::Audio::speaker_bits(Speaker::FrontLeft) | Lowl::Audio::speaker_bits(Speaker::FrontRight));
        REQUIRE_EQ(stereo, ChannelLayout::Stereo);
        REQUIRE_FALSE(ChannelLayout::from_mask(0).is_valid());

        const ChannelLayout layout = ChannelLayout::Surround_5_1;
        REQUIRE_EQ(layout.speaker_at(0), Speaker::FrontLeft);
        REQUIRE_EQ(layout.speaker_at(1), Speaker::FrontRight);
        REQUIRE_EQ(layout.speaker_at(2), Speaker::FrontCenter);
        REQUIRE_EQ(layout.speaker_at(3), Speaker::LowFrequency);
        REQUIRE_EQ(layout.speaker_at(4), Speaker::SideLeft);
        REQUIRE_EQ(layout.speaker_at(5), Speaker::SideRight);
        REQUIRE_EQ(layout.speaker_at(6), static_cast<Speaker>(0));

        for (uint8_t channel_index = 0; channel_index < layout.channel_count; channel_index++) {
            const Speaker speaker = layout.speaker_at(channel_index);
            REQUIRE_EQ(layout.index_of(speaker), static_cast<int>(channel_index));
            REQUIRE(layout.has(speaker));
        }
        REQUIRE_EQ(layout.index_of(Speaker::BackLeft), -1);
        REQUIRE_FALSE(layout.has(Speaker::BackLeft));
    }

    SUBCASE("named layouts preserve distinct layouts with identical counts") {
        REQUIRE_EQ(ChannelLayout::Surround_5_1, ChannelLayout::from_count(6));
        REQUIRE_NE(ChannelLayout::Surround_5_1, ChannelLayout::Surround_5_1_Rear);
        REQUIRE_EQ(ChannelLayout::Stereo.to_string(), "Stereo");
        REQUIRE_EQ(ChannelLayout{}.to_string(), "Invalid");
    }
}
