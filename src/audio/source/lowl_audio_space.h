#ifndef LOWL_AUDIO_SPACE_H
#define LOWL_AUDIO_SPACE_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "./lowl_typedef.h"
#include "audio/backend/lowl_audio_device.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_mixer.h"
#include "audio/source/lowl_audio_voice.h"

namespace Lowl::Audio {
    /**
     * Owns registered audio assets and explicit playback instances that render through an internal mixer.
     *
     * `render()` is the real-time render path and does not take this class' state lock. All other
     * methods that touch asset or playback bookkeeping synchronize on `state_mutex`; they are safe to
     * call concurrently, but they are not real-time safe and should stay off the audio callback
     * thread.
     */
    class AudioSpace : public AudioSource {
    public:
        static constexpr AudioAssetId InvalidAudioAssetId = 0;
        static constexpr AudioPlaybackId InvalidAudioPlaybackId = 0;

    private:
        static constexpr AudioAssetId FirstAudioAssetId = 1;
        static constexpr AudioPlaybackId FirstAudioPlaybackId = 1;
        static constexpr int LookupGrowth = 100;

        enum class PlaybackState : uint8_t {
            Free = 0,
            Stopped = 1,
            Playing = 2,
            Paused = 3,
            Retiring = 4,
        };

        struct PlaybackSlot {
            std::unique_ptr<AudioVoice> voice;
            AudioAssetId audio_asset_id = InvalidAudioAssetId;
            uint16_l generation = 1;
            PlaybackState state = PlaybackState::Free;
        };

        std::vector<std::shared_ptr<AudioData>> audio_asset_lookup;
        std::vector<PlaybackSlot> playback_lookup;
        mutable std::mutex state_mutex;
        std::unique_ptr<AudioMixer> mixer;
        uint16_l mixer_owner_id;
        AudioAssetId current_audio_asset_id;
        AudioPlaybackId current_audio_playback_id;

        AudioAssetId insert_audio_asset_locked(std::shared_ptr<AudioData> p_audio_data);
        AudioPlaybackId insert_playback_locked(std::unique_ptr<AudioVoice> p_voice, AudioAssetId p_audio_asset_id);
        void recycle_playback_locked(PlaybackSlot &p_slot);

        void collect_garbage_locked();

        std::shared_ptr<AudioData> get_audio_asset_locked(AudioAssetId p_audio_asset_id) const;
        AudioMixerHandle get_mixer_handle_locked(AudioPlaybackId p_audio_playback_id) const;
        PlaybackSlot *get_playback_slot_locked(AudioPlaybackId p_audio_playback_id);
        const PlaybackSlot *get_playback_slot_locked(AudioPlaybackId p_audio_playback_id) const;

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        RenderResult render(AudioBlockView p_block) override;

        void play(AudioPlaybackId p_audio_playback_id);

        void pause(AudioPlaybackId p_audio_playback_id);

        void resume(AudioPlaybackId p_audio_playback_id);

        void stop(AudioPlaybackId p_audio_playback_id);

        AudioAssetId add_audio(const std::string &p_path, Error &error);

        AudioAssetId add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error);

        AudioPlaybackId create_playback(AudioAssetId p_audio_asset_id);

        std::map<AudioAssetId, std::string> get_name_mapping() const;

        void clear_all_audio();

        void stop_all_audio();

        virtual size_l get_frames_remaining(AudioPlaybackId p_audio_playback_id) const;

        virtual size_l get_frame_count(AudioPlaybackId p_audio_playback_id) const;

        virtual size_l get_frame_position(AudioPlaybackId p_audio_playback_id) const;

        void set_volume(AudioPlaybackId p_audio_playback_id, Volume p_volume);

        void set_panning(AudioPlaybackId p_audio_playback_id, Panning p_panning);

        void reset(AudioPlaybackId p_audio_playback_id);

        void seek_time(AudioPlaybackId p_audio_playback_id, double_l p_seconds);

        void seek_frame(AudioPlaybackId p_audio_playback_id, size_t p_frame);

        AudioSpace(SampleRate p_sample_rate, AudioChannel p_channel);

        ~AudioSpace() override;
    };
} // namespace Lowl::Audio

#endif
