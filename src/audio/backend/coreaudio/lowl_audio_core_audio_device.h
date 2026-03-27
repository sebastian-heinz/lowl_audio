#ifndef LOWL_AUDIO_CORE_AUDIO_DEVICE_H
#define LOWL_AUDIO_CORE_AUDIO_DEVICE_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>

#include "audio/backend/lowl_audio_device.h"

namespace Lowl::Audio {
    class CoreAudioDevice : public AudioDevice {
    private:
        AudioObjectID device_id;
        AudioUnit _Nullable audio_unit;
        pid_t hog_pid;
        bool device_property_listener_registered;
        bool running_listener_registered;
        bool audio_unit_initialized;

        static std::vector<Lowl::Audio::AudioDeviceProperties> create_device_properties(AudioObjectID p_device_id);

        static AudioUnit _Nullable create_audio_unit(AudioObjectID p_device_id, Error &error);

        static AudioStreamBasicDescription create_description(AudioDeviceProperties p_device_id);

        static bool test_device_properties(AudioObjectID p_device_id,
                                           AudioUnit _Nullable p_audio_unit,
                                           AudioDeviceProperties p_properties,
                                           bool silent = true);

        void cleanup_audio_unit(Error &error);

        void cleanup_failed_start();

        void release_hog();

    public:
        static std::unique_ptr<CoreAudioDevice>
        construct(const std::string &p_driver_name, AudioObjectID p_device_id, Error &error);

        OSStatus audio_callback(AudioUnitRenderActionFlags *_Nonnull ioActionFlags,
                                const AudioTimeStamp *_Nonnull inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList *_Nullable ioData);

        void start_stop_callback(AudioUnit _Nonnull inUnit,
                                 AudioUnitPropertyID inID,
                                 AudioUnitScope inScope,
                                 AudioUnitElement inElement);

        OSStatus property_callback(AudioObjectID inObjectID,
                                   UInt32 inNumberAddresses,
                                   const AudioObjectPropertyAddress *_Nonnull inAddresses);

        virtual void start(AudioDeviceProperties p_audio_device_properties,
                           std::shared_ptr<AudioSource> p_audio_source,
                           Error &error) override;

        virtual void stop(Error &error) override;

        CoreAudioDevice(_constructor_tag);

        ~CoreAudioDevice() override;
    };
} // namespace Lowl::Audio

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_DEVICE_H */
