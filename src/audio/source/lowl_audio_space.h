#ifndef LOWL_AUDIO_SPACE_H
#define LOWL_AUDIO_SPACE_H

#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio/source/lowl_audio_asset_handle.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_mixer.h"
#include "audio/source/lowl_audio_playback_handle.h"
#include "audio/source/lowl_audio_voice.h"
#include "lowl_error.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Game-oriented polyphonic source backed by registered, render-ready audio assets.
     *
     * Assets are converted to the Space format when they are added. Playback instances are
     * controlled through generation-safe IDs and mixed by exactly one internal AudioMixer.
     * AudioSpace owns no graph topology: callers route the Space itself into any external Mixer
     * hierarchy they need.
     *
     * The render path never takes state_mutex. Asset and playback management is control-thread
     * work and is not real-time safe.
     */
    class AudioSpace : public AudioSource {
    public:
        static constexpr AudioAssetId InvalidAudioAssetId = 0;
        static constexpr AudioAssetHandle InvalidAudioAssetHandle = {};
        static constexpr AudioPlaybackHandle InvalidAudioPlaybackHandle = {};

        using AudioSource::pause;
        using AudioSource::play;
        using AudioSource::set_panning;
        using AudioSource::set_volume;

    private:
        static constexpr AudioAssetId FirstAudioAssetId = 1;
        static constexpr AudioPlaybackId InvalidPlaybackSlotId = 0;
        static constexpr AudioPlaybackId FirstPlaybackSlotId = 1;
        static constexpr int LookupGrowth = 100;
        static constexpr size_l LiveFrameCountSentinel = 1;

        enum class SlotState : uint8_t {
            Active = 0,
            Retiring = 1,
        };

        struct AssetSlot {
            std::shared_ptr<AudioData> audio_data;
            AudioGeneration generation = 1;
        };

        struct PlaybackSlot {
            std::unique_ptr<AudioVoice> voice;
            AudioAssetHandle audio_asset_handle = InvalidAudioAssetHandle;
            AudioMixerHandle mixer_handle{};
            AudioGeneration generation = 1;
            SlotState slot_state = SlotState::Active;
        };

        std::vector<AssetSlot> audio_asset_lookup;
        std::vector<PlaybackSlot> playback_lookup;
        std::array<AudioPlaybackId, AudioMixer::MaxConnections + 1> mixer_playback_lookup{};
        std::vector<AudioAssetId> free_audio_asset_slots;
        std::vector<AudioPlaybackId> free_playback_slots;
        mutable std::mutex state_mutex;
        AudioMixer mixer;
        AudioInstanceId owner_id;
        AudioAssetId current_audio_asset_id;
        AudioPlaybackId current_audio_playback_slot_id;

        AudioAssetHandle insert_audio_asset_locked(std::shared_ptr<AudioData> p_audio_data);
        AudioPlaybackHandle insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                   AudioAssetHandle p_audio_asset_handle);
        void recycle_audio_asset_locked(AudioAssetId p_asset_id, AssetSlot &p_slot);
        void recycle_playback_locked(AudioPlaybackId p_slot_id, PlaybackSlot &p_slot);
        bool connect_playback_locked(AudioPlaybackId p_slot_id, PlaybackSlot &p_slot, Error &p_error);
        void clear_mixer_connection_locked(AudioPlaybackId p_slot_id, PlaybackSlot &p_slot);
        bool retire_playback_locked(AudioPlaybackId p_slot_id, PlaybackSlot &p_slot, Error &p_error);
        void collect_mixer_completions_locked();

        std::shared_ptr<AudioData> get_audio_asset_locked(AudioAssetHandle p_audio_asset_handle) const;
        AudioPlaybackId find_playback_slot_id_by_mixer_handle_locked(AudioMixerHandle p_mixer_handle) const;
        PlaybackSlot *get_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle);
        const PlaybackSlot *get_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle) const;
        PlaybackSlot *require_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle,
                                                   Error &p_error,
                                                   const char *p_error_message);
        const PlaybackSlot *require_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle,
                                                         Error &p_error,
                                                         const char *p_error_message) const;

    public:
        size_l get_frames_remaining() const override;
        size_l get_frame_position() const override;
        size_l get_frame_count() const override;

        RenderResult mix_into(AudioBlockView p_block,
                              const MixGainVector &p_upstream_gain) override;

        AudioAssetHandle add_audio(const std::string &p_path, Error &p_error);
        AudioAssetHandle add_audio(std::unique_ptr<AudioData> p_audio_data, Error &p_error);
        void remove_audio(AudioAssetHandle p_audio_asset_handle, Error &p_error);

        [[nodiscard]] AudioPlaybackHandle create_playback(AudioAssetHandle p_audio_asset_handle, Error &p_error);
        [[nodiscard]] AudioPlaybackHandle play_asset(AudioAssetHandle p_audio_asset_handle, Error &p_error);
        void destroy_playback(AudioPlaybackHandle p_audio_playback_handle, Error &p_error);

        void play(AudioPlaybackHandle p_audio_playback_handle, Error &p_error);
        /** Pausing or stopping keeps the playback's fixed mixer connection reserved. */
        void pause(AudioPlaybackHandle p_audio_playback_handle, Error &p_error);
        void resume(AudioPlaybackHandle p_audio_playback_handle, Error &p_error);
        void stop(AudioPlaybackHandle p_audio_playback_handle, Error &p_error);

        void clear_all_audio(Error &p_error);
        void stop_all_audio();

        size_l get_frames_remaining(AudioPlaybackHandle p_audio_playback_handle, Error &p_error) const;
        size_l get_frame_count(AudioPlaybackHandle p_audio_playback_handle, Error &p_error) const;
        size_l get_frame_position(AudioPlaybackHandle p_audio_playback_handle, Error &p_error) const;

        void set_volume(AudioPlaybackHandle p_audio_playback_handle, Volume p_volume, Error &p_error);
        void set_panning(AudioPlaybackHandle p_audio_playback_handle, Panning p_panning, Error &p_error);
        void reset(AudioPlaybackHandle p_audio_playback_handle, Error &p_error);
        void seek_time(AudioPlaybackHandle p_audio_playback_handle, double_l p_seconds, Error &p_error);
        void seek_frame(AudioPlaybackHandle p_audio_playback_handle, size_t p_frame, Error &p_error);

        std::map<AudioAssetId, std::string> get_name_mapping() const;

        explicit AudioSpace(AudioFormat p_audio_format);
        ~AudioSpace() override = default;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_SPACE_H
