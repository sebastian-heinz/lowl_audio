#include <doctest/doctest.h>

#ifdef LOWL_DRIVER_CORE_AUDIO

#include <vector>

#include "audio/backend/coreaudio/lowl_audio_core_audio_device.h"

namespace {
    class RecordingCoreAudioDevice final : public Lowl::Audio::CoreAudioDevice {
    public:
        RecordingCoreAudioDevice() : CoreAudioDevice(_constructor_tag{}) {
        }

        const std::vector<AudioObjectPropertySelector> &get_seen_selectors() const {
            return seen_selectors;
        }

    protected:
        void handle_property_address(const AudioObjectPropertyAddress &p_address) override {
            seen_selectors.push_back(p_address.mSelector);
            CoreAudioDevice::handle_property_address(p_address);
        }

    private:
        std::vector<AudioObjectPropertySelector> seen_selectors;
    };
} // namespace

TEST_CASE("CoreAudioDevice") {
    SUBCASE("CoreAudioDevice - property_callback processes every address in the batch") {
        RecordingCoreAudioDevice device;
        const AudioObjectPropertyAddress addresses[] = {
            {kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
            {kAudioDeviceProcessorOverload, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain},
            {kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain},
        };

        REQUIRE_EQ(device.property_callback(kAudioObjectSystemObject, 3, addresses), noErr);
        REQUIRE_EQ(device.get_seen_selectors().size(), 3);
        REQUIRE_EQ(device.get_seen_selectors()[0], kAudioObjectPropertyName);
        REQUIRE_EQ(device.get_seen_selectors()[1], kAudioDeviceProcessorOverload);
        REQUIRE_EQ(device.get_seen_selectors()[2], kAudioDevicePropertyNominalSampleRate);
    }

    SUBCASE("CoreAudioDevice - property_callback ignores empty batches safely") {
        RecordingCoreAudioDevice device;
        const AudioObjectPropertyAddress dummy_address = {
            kAudioObjectPropertyName,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain,
        };

        REQUIRE_EQ(device.property_callback(kAudioObjectSystemObject, 0, &dummy_address), noErr);
        REQUIRE(device.get_seen_selectors().empty());
    }
}

#endif
