#ifndef LOWL_AUDIO_SPACE_H
#define LOWL_AUDIO_SPACE_H

#include <map>
#include <mutex>
#include <string>

#include "audio/backend/lowl_audio_device.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_mixer.h"
#include "audio/source/lowl_audio_voice.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Owns a collection of clips addressed by `SpaceId` and plays them through an internal mixer.
     *
     * `render()` is the real-time render path and does not take this class' state lock. All other
     * methods that touch clip or voice bookkeeping synchronize on `state_mutex`; they are safe to
     * call concurrently, but they are not real-time safe and should stay off the audio callback
     * thread.
     */
    class AudioSpace : public AudioSource {
    public:
        static const SpaceId InvalidSpaceId = 0;

    private:
        static const SpaceId FirstSpaceId = 1;
        static const int LookupGrowth = 100;

        std::vector<std::shared_ptr<AudioData>> audio_data_lookup;
        mutable std::mutex state_mutex;
        mutable std::vector<std::vector<std::shared_ptr<AudioVoice>>> active_voice_lookup;
        mutable std::vector<std::shared_ptr<AudioVoice>> retired_voices;
        std::unique_ptr<AudioMixer> mixer;
        SpaceId current_id;

        SpaceId insert_audio_data(std::shared_ptr<AudioData> p_audio_data);

        void collect_garbage_locked() const;

        std::shared_ptr<AudioVoice> get_latest_voice_locked(SpaceId p_id) const;

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        RenderResult render(AudioBlockView p_block) override;

        void play(SpaceId p_id, Volume p_volume, Panning p_panning) const;

        void play(SpaceId p_id) const;

        void stop(SpaceId p_id) const;

        SpaceId add_audio(const std::string &p_path, Error &error);

        SpaceId add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error);

        std::map<SpaceId, std::string> get_name_mapping() const;

        void clear_all_audio();

        void stop_all_audio();

        virtual size_l get_frames_remaining(SpaceId p_id) const;

        virtual size_l get_frame_count(SpaceId p_id) const;

        virtual size_l get_frame_position(SpaceId p_id) const;

        void set_volume(SpaceId p_id, Volume p_volume) const;

        void set_panning(SpaceId p_id, Panning p_panning) const;

        void reset(SpaceId p_id) const;

        void seek_time(SpaceId p_id, double_l p_seconds) const;

        void seek_frame(SpaceId p_id, size_t p_frame) const;

        AudioSpace(SampleRate p_sample_rate, AudioChannel p_channel);

        ~AudioSpace() override;

    private:
        std::shared_ptr<AudioData> get_audio_data_locked(SpaceId p_id) const;
    };
} // namespace Lowl::Audio

#endif
