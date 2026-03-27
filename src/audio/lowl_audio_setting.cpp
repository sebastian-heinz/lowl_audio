#include "lowl_audio_setting.h"

std::vector<double> Lowl::Audio::AudioSetting::test_sample_rates = {
    8000.0, 9600.0, 11025.0, 12000.0, 16000.0, 22050.0, 24000.0, 32000.0, 44100.0, 48000.0, 88200.0, 96000.0, 192000.0};

std::vector<Lowl::Audio::SampleFormat> Lowl::Audio::AudioSetting::test_sample_formats = {SampleFormat::FLOAT_32,
                                                                                         SampleFormat::INT_32,
                                                                                         SampleFormat::INT_24,
                                                                                         SampleFormat::INT_16,
                                                                                         SampleFormat::INT_8,
                                                                                         SampleFormat::U_INT_8};

std::vector<Lowl::Audio::ChannelLayout> Lowl::Audio::AudioSetting::test_channel_layouts = {
    ChannelLayout::Mono,
    ChannelLayout::Stereo,
    ChannelLayout::Surround_3_0,
    ChannelLayout::Quad,
    ChannelLayout::Quad_Side,
    ChannelLayout::Surround_4_0,
    ChannelLayout::Surround_5_0,
    ChannelLayout::Surround_5_0_Rear,
    ChannelLayout::Surround_5_1,
    ChannelLayout::Surround_5_1_Rear,
    ChannelLayout::Surround_6_1,
    ChannelLayout::Surround_7_1,
    ChannelLayout::Surround_7_1_Front,
};

std::vector<Lowl::Audio::SampleFormat> Lowl::Audio::AudioSetting::get_test_sample_formats() {
    return test_sample_formats;
}

std::vector<double> Lowl::Audio::AudioSetting::get_test_sample_rates() {
    return test_sample_rates;
}

std::vector<Lowl::Audio::ChannelLayout> Lowl::Audio::AudioSetting::get_test_channel_layouts() {
    return test_channel_layouts;
}
