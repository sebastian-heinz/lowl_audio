#ifndef LOWL_AUDIO_CORE_AUDIO_DEVICE_H
#define LOWL_AUDIO_CORE_AUDIO_DEVICE_H

#ifdef LOWL_DRIVER_CORE_AUDIO

#include "audio/lowl_audio_device.h"

#include <CoreAudio/AudioHardware.h>
#include <AudioUnit/AudioUnit.h>

namespace Lowl::Audio {

    class CoreAudioDevice : public AudioDevice {

    private:
        AudioObjectID device_id;
        Lowl::SampleRate default_sample_rate;
        Lowl::Audio::AudioChannel output_channel;
        uint32_t input_stream_count;
        uint32_t output_stream_count;
        AudioUnit _Nullable audio_unit;
        pid_t hog_pid;

        AudioDeviceProperties audio_device_properties{};

        SampleCount set_frames_per_buffer(SampleCount p_frames_per_buffer, Lowl::Error &error);
        SampleRate set_sample_rate(SampleRate p_sample_rate, Lowl::Error &error);

        static std::vector<Lowl::Audio::AudioDeviceProperties> create_device_properties(AudioObjectID p_device_id);
        static AudioUnit _Nullable create_audio_unit(AudioObjectID p_device_id, Error &error);
        static AudioStreamBasicDescription create_description(AudioDeviceProperties p_device_id);

        void release_hog();

    public:
        static std::unique_ptr<CoreAudioDevice> construct(
                const std::string &p_driver_name,
                AudioObjectID p_device_id,
                Error &error
        );

        OSStatus audio_callback(
                AudioUnitRenderActionFlags *_Nonnull ioActionFlags,
                const AudioTimeStamp *_Nonnull inTimeStamp,
                UInt32 inBusNumber,
                UInt32 inNumberFrames,
                AudioBufferList *_Nullable ioData
        );

        void start_stop_callback(
                AudioUnit _Nonnull inUnit,
                AudioUnitPropertyID inID,
                AudioUnitScope inScope,
                AudioUnitElement inElement
        );

        OSStatus property_callback(
                AudioObjectID inObjectID,
                UInt32 inNumberAddresses,
                const AudioObjectPropertyAddress *_Nonnull inAddresses
        );

        virtual void start(AudioDeviceProperties p_audio_device_properties,
                           std::shared_ptr<AudioSource> p_audio_source,
                           Error &error) override;

        virtual void stop(Error &error) override;

        CoreAudioDevice(_constructor_tag);

        ~CoreAudioDevice() override;
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_DEVICE_H */
