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

        SampleCount set_frames_per_buffer(SampleCount p_frames_per_buffer, Lowl::Error &error);
        SampleRate set_sample_rate(SampleRate p_sample_rate, Lowl::Error &error);

        CoreAudioDevice();

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

        void start(std::shared_ptr<AudioSource> p_audio_source, Error &error) override;

        void stop(Error &error) override;

        bool is_supported(Lowl::Audio::AudioChannel p_channel,
                          Lowl::SampleRate p_sample_rate,
                          SampleFormat p_sample_format,
                          Error &error
        ) override;

        Lowl::SampleRate get_default_sample_rate() override;

        void set_exclusive_mode(bool p_exclusive_mode, Error &error) override;

        ~CoreAudioDevice();
    };
}

#endif /* LOWL_DRIVER_CORE_AUDIO */
#endif /* LOWL_AUDIO_CORE_AUDIO_DEVICE_H */
