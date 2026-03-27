#ifndef LOWL_AUDIO_SPACE_H
#define LOWL_AUDIO_SPACE_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "./lowl_typedef.h"
#include "audio/backend/lowl_audio_device.h"
#include "audio/source/lowl_audio_asset_handle.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_mixer.h"
#include "audio/source/lowl_audio_playback_handle.h"
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
        static constexpr AudioAssetHandle InvalidAudioAssetHandle = {};
        static constexpr AudioPlaybackHandle InvalidAudioPlaybackHandle = {};

    private:
        static constexpr AudioAssetId FirstAudioAssetId = 1;
        static constexpr AudioPlaybackId InvalidPlaybackSlotId = 0;
        static constexpr AudioPlaybackId FirstPlaybackSlotId = 1;
        static constexpr int LookupGrowth = 100;

        enum class SlotState : uint8_t {
            Active = 0,
            Retiring = 1,
        };

        struct PlaybackSlot {
            std::unique_ptr<AudioVoice> voice;
            AudioAssetHandle audio_asset_handle = InvalidAudioAssetHandle;
            uint16_l generation = 1;
            SlotState slot_state = SlotState::Active;
        };

        struct AssetSlot {
            std::shared_ptr<AudioData> audio_data;
            uint16_l generation = 1;
        };

        std::vector<AssetSlot> audio_asset_lookup;
        std::vector<PlaybackSlot> playback_lookup;
        std::vector<AudioAssetId> free_audio_asset_slots;
        std::vector<AudioPlaybackId> free_playback_slots;
        mutable std::mutex state_mutex;
        std::unique_ptr<AudioMixer> mixer;
        uint32_l owner_id;
        uint16_l mixer_owner_id;
        AudioAssetId current_audio_asset_id;
        AudioPlaybackId current_audio_playback_slot_id;

        AudioAssetHandle insert_audio_asset_locked(std::shared_ptr<AudioData> p_audio_data);
        AudioPlaybackHandle insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                   AudioAssetHandle p_audio_asset_handle);
        void recycle_audio_asset_locked(AudioAssetId p_asset_id, AssetSlot &p_slot);
        void recycle_playback_locked(AudioPlaybackId p_slot_id, PlaybackSlot &p_slot);

        void drain_mixer_acks_locked();

        std::shared_ptr<AudioData> get_audio_asset_locked(AudioAssetHandle p_audio_asset_handle) const;
        AudioMixerHandle get_mixer_handle_locked(AudioPlaybackHandle p_audio_playback_handle) const;
        PlaybackSlot *get_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle);
        const PlaybackSlot *get_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle) const;

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        RenderResult render(AudioBlockView p_block) override;

        void play(AudioPlaybackHandle p_audio_playback_handle);

        void pause(AudioPlaybackHandle p_audio_playback_handle);

        void resume(AudioPlaybackHandle p_audio_playback_handle);

        void stop(AudioPlaybackHandle p_audio_playback_handle);

        AudioAssetHandle add_audio(const std::string &p_path, Error &error);

        AudioAssetHandle add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error);

        AudioPlaybackHandle create_playback(AudioAssetHandle p_audio_asset_handle);

        std::map<AudioAssetId, std::string> get_name_mapping() const;

        void clear_all_audio();

        void stop_all_audio();

        virtual size_l get_frames_remaining(AudioPlaybackHandle p_audio_playback_handle) const;

        virtual size_l get_frame_count(AudioPlaybackHandle p_audio_playback_handle) const;

        virtual size_l get_frame_position(AudioPlaybackHandle p_audio_playback_handle) const;

        void set_volume(AudioPlaybackHandle p_audio_playback_handle, Volume p_volume);

        void set_panning(AudioPlaybackHandle p_audio_playback_handle, Panning p_panning);

        void reset(AudioPlaybackHandle p_audio_playback_handle);

        void seek_time(AudioPlaybackHandle p_audio_playback_handle, double_l p_seconds);

        void seek_frame(AudioPlaybackHandle p_audio_playback_handle, size_t p_frame);

        AudioSpace(SampleRate p_sample_rate,
                   ChannelLayout p_channel_layout,
                   uint32_t p_mixer_scratch_buffer_capacity = AudioMixer::DefaultScratchBufferCapacity);

        ~AudioSpace() override;
    };
} // namespace Lowl::Audio

#endif
