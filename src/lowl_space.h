#ifndef LOWL_SPACE_H
#define LOWL_SPACE_H

#include "lowl_typedef.h"
#include "lowl_audio_data.h"
#include "lowl_audio_mixer.h"
#include "lowl_device.h"

#include <string>

namespace Lowl {

    /**
     * A "Space" represents a set of audio files that are managed by an Id.
     *
     * 1) use add_audio() to provide audio for playback
     * 2) set_sample_rate() and set_channel() if you want to enforce a specific config,
     *    this will cause all mismatched data to be resampled.
     *    Otherwise the sample rate with the least required resampling will be chosen.
     *    NOTE: if the playback device is not set to the same configuration, additional
     *          resampling might be required.
     * 3) load() will perform all processing, this might take a little bit.
     *    after it has been called, no more audio can be added and no further changes are possible.
     * 4) use get_out_stream() to retrieve the stream where the audio will be written to.
     *    pass this to the device or a mixer for playback.
     * 5) use play() and stop() to produce the sounds
     */
    class Space : public AudioSource {
    public:
        static const SpaceId InvalidSpaceId = 0;

    private:
        std::vector<std::shared_ptr<AudioData>> audio_data_lookup;
        std::unique_ptr<AudioMixer> mixer;
        SpaceId current_id;

    public:
        virtual size_l get_frames_remaining() const override;

        virtual size_l get_frame_position() const override;

        virtual ReadResult read(AudioFrame &audio_frame) override;

        void play(SpaceId p_id, Volume p_volume, Panning p_panning) const;

        void play(SpaceId p_id) const;

        void stop(SpaceId p_id) const;

        SpaceId add_audio(const std::string &p_path, Error &error);

        SpaceId add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error);

        virtual size_l get_frames_remaining(SpaceId p_id) const;

        virtual size_l get_frame_position(SpaceId p_id) const;

        void set_volume(SpaceId p_id, Volume p_volume) const;

        void set_panning(SpaceId p_id, Panning p_panning) const;

        void reset(SpaceId p_id) const;

        void seek_time(SpaceId p_id, double_l p_seconds) const;

        void seek_frame(SpaceId p_id, size_t p_frame) const;

        Space(SampleRate p_sample_rate, Channel p_channel);

        ~Space();

    private:
        std::shared_ptr<AudioData> get_audio_data(SpaceId p_id) const;
    };
}

#endif