#ifndef LOWL_SPACE_H
#define LOWL_SPACE_H

#include "lowl_typedef.h"
#include "lowl_audio_data.h"
#include "lowl_audio_mixer.h"

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
    class Space {
    public:
        static const Lowl::SpaceId InvalidSpaceId = 0;

    private:
        std::vector<std::shared_ptr<AudioData>> audio_data_lookup;
        std::shared_ptr<AudioMixer> mixer;
        Lowl::SpaceId current_id;
        bool is_loaded;
        SampleRate sample_rate;
        Channel channel;

    public:
        void play(Lowl::SpaceId p_id);

        void stop(Lowl::SpaceId p_id);

        Lowl::SpaceId add_audio(const std::string &p_path, Error &error);

        Lowl::SpaceId add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error);

        void load();

        void set_sample_rate(SampleRate p_sample_rate);

        void set_channel(Channel p_channel);

        std::shared_ptr<Lowl::AudioMixer> get_mixer();

        Space();

        ~Space();
    };
}

#endif