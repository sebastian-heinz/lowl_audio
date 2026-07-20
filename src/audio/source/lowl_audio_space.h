#ifndef LOWL_AUDIO_SPACE_H
#define LOWL_AUDIO_SPACE_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "./lowl_typedef.h"
#include "audio/source/lowl_audio_asset_handle.h"
#include "audio/source/lowl_audio_bus.h"
#include "audio/source/lowl_audio_bus_handle.h"
#include "audio/source/lowl_audio_bus_slot_handle.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_playback_handle.h"
#include "audio/source/lowl_audio_stream.h"
#include "audio/source/lowl_audio_stream_handle.h"
#include "audio/source/lowl_audio_voice.h"

namespace Lowl::Audio {
    /**
     * High-level facade for an internal bus tree rooted at a master mixer.
     *
     * Audio assets are registered once, then instantiated as explicit playback handles that can be
     * routed to the master bus or to child buses created with `create_bus()`. Each playback and bus
     * keeps independent gain and panning state while still rendering through the same hot
     * `mix_into()` accumulation path.
     *
     * `render()` is the real-time render path and does not take this class' state lock. All other
     * methods that touch asset, playback, or bus bookkeeping synchronize on `state_mutex`; they are
     * safe to call concurrently, but they are not real-time safe and should stay off the audio
     * callback thread.
     *
     * As an aggregate live source, this class has no finite total frame count. Its aggregate
     * `get_frames_remaining()` / `get_frame_count()` queries therefore use a one-frame sentinel so
     * callers can treat it as renderable without observing `frames_remaining > frame_count`.
     */
    class AudioSpace : public AudioSource {
    public:
        static constexpr AudioAssetId InvalidAudioAssetId = 0;
        static constexpr AudioAssetHandle InvalidAudioAssetHandle = {};
        static constexpr AudioBusHandle InvalidAudioBusHandle = {};
        static constexpr AudioPlaybackHandle InvalidAudioPlaybackHandle = {};
        static constexpr AudioStreamHandle InvalidAudioStreamHandle = {};

        using AudioSource::pause;
        using AudioSource::play;
        using AudioSource::set_panning;
        using AudioSource::set_volume;

    private:
        static constexpr AudioAssetId FirstAudioAssetId = 1;
        static constexpr AudioBusId InvalidBusSlotId = 0;
        static constexpr AudioBusId MasterBusSlotId = 1;
        static constexpr AudioBusId FirstDynamicBusSlotId = 2;
        static constexpr AudioPlaybackId InvalidPlaybackSlotId = 0;
        static constexpr AudioPlaybackId FirstPlaybackSlotId = 1;
        static constexpr AudioStreamId InvalidStreamSlotId = 0;
        static constexpr AudioStreamId FirstStreamSlotId = 1;
        static constexpr int LookupGrowth = 100;
        static constexpr size_l LiveFrameCountSentinel = 1;

        enum class SlotState : uint8_t {
            Active = 0,
            Retiring = 1,
        };

        struct PlaybackSlot {
            std::unique_ptr<AudioVoice> voice;
            AudioAssetHandle audio_asset_handle = InvalidAudioAssetHandle;
            AudioBusHandle audio_bus_handle = InvalidAudioBusHandle;
            AudioBusSlotHandle bus_slot_handle{};
            uint16_l generation = 1;
            SlotState slot_state = SlotState::Active;
            bool bus_submission_started = false;
        };

        struct BusSlot {
            std::unique_ptr<AudioBus> bus;
            AudioBusHandle parent_bus_handle = InvalidAudioBusHandle;
            AudioBusSlotHandle bus_slot_handle{};
            uint16_l generation = 1;
            SlotState slot_state = SlotState::Active;
            bool bus_submission_started = false;
            std::vector<AudioPlaybackId> playback_lookup;
            std::vector<AudioBusId> child_bus_lookup;
            std::vector<AudioStreamId> stream_lookup;
        };

        struct StreamSlot {
            std::unique_ptr<AudioStream> stream;
            AudioBusHandle audio_bus_handle = InvalidAudioBusHandle;
            AudioBusSlotHandle bus_slot_handle{};
            uint16_l generation = 1;
            SlotState slot_state = SlotState::Active;
            bool bus_submission_started = false;
        };

        struct AssetSlot {
            std::shared_ptr<AudioData> audio_data;
            uint16_l generation = 1;
        };

        std::vector<AssetSlot> audio_asset_lookup;
        std::vector<BusSlot> bus_lookup;
        std::vector<PlaybackSlot> playback_lookup;
        std::vector<StreamSlot> stream_lookup;
        std::vector<AudioPlaybackId> root_playback_lookup;
        std::vector<AudioBusId> root_bus_lookup;
        std::vector<AudioStreamId> root_stream_lookup;
        std::vector<AudioAssetId> free_audio_asset_slots;
        std::vector<AudioBusId> free_bus_slots;
        std::vector<AudioPlaybackId> free_playback_slots;
        std::vector<AudioStreamId> free_stream_slots;
        mutable std::mutex state_mutex;
        std::unique_ptr<AudioBus> master_bus_node;
        uint32_l owner_id;
        AudioAssetId current_audio_asset_id;
        AudioBusId current_audio_bus_slot_id;
        AudioPlaybackId current_audio_playback_slot_id;
        AudioStreamId current_audio_stream_slot_id;

        AudioAssetHandle insert_audio_asset_locked(std::shared_ptr<AudioData> p_audio_data);
        AudioBusHandle insert_bus_locked(AudioBusHandle p_parent_bus_handle);
        AudioPlaybackHandle insert_playback_locked(std::unique_ptr<AudioVoice> p_voice,
                                                   AudioAssetHandle p_audio_asset_handle,
                                                   AudioBusHandle p_audio_bus_handle);
        AudioStreamHandle insert_stream_locked(std::unique_ptr<AudioStream> p_stream,
                                               AudioBusHandle p_audio_bus_handle);
        void recycle_audio_asset_locked(AudioAssetId p_asset_id, AssetSlot &p_slot);
        void recycle_bus_locked(AudioBusId p_bus_id, BusSlot &p_slot);
        void recycle_playback_locked(AudioPlaybackId p_slot_id, PlaybackSlot &p_slot);
        void recycle_stream_locked(AudioStreamId p_slot_id, StreamSlot &p_slot);
        void retire_bus_locked(AudioBusId p_bus_id, BusSlot &p_slot);
        void retire_playback_locked(AudioPlaybackId p_slot_id, PlaybackSlot &p_slot);
        void retire_stream_locked(AudioStreamId p_slot_id, StreamSlot &p_slot);

        void drain_all_bus_acks_locked();
        void drain_bus_acks_locked(AudioBusHandle p_audio_bus_handle);

        std::shared_ptr<AudioData> get_audio_asset_locked(AudioAssetHandle p_audio_asset_handle) const;
        AudioBusHandle normalize_bus_handle_locked(AudioBusHandle p_audio_bus_handle) const;
        AudioBus *get_bus_node_locked(AudioBusHandle p_audio_bus_handle);
        const AudioBus *get_bus_node_locked(AudioBusHandle p_audio_bus_handle) const;
        AudioPlaybackId find_playback_slot_id_by_bus_slot_handle_locked(AudioBusHandle p_audio_bus_handle,
                                                                        AudioBusSlotHandle p_bus_slot_handle) const;
        AudioBusId find_child_bus_slot_id_by_bus_slot_handle_locked(AudioBusHandle p_audio_bus_handle,
                                                                    AudioBusSlotHandle p_bus_slot_handle) const;
        void map_playback_handle_locked(AudioBusHandle p_audio_bus_handle,
                                        AudioBusSlotHandle p_bus_slot_handle,
                                        AudioPlaybackId p_playback_slot_id);
        void unmap_playback_handle_locked(AudioBusHandle p_audio_bus_handle, AudioBusSlotHandle p_bus_slot_handle);
        void map_bus_handle_locked(AudioBusHandle p_audio_bus_handle, AudioBusSlotHandle p_bus_slot_handle,
                                   AudioBusId p_bus_id);
        void unmap_bus_handle_locked(AudioBusHandle p_audio_bus_handle, AudioBusSlotHandle p_bus_slot_handle);
        void map_stream_handle_locked(AudioBusHandle p_audio_bus_handle,
                                      AudioBusSlotHandle p_bus_slot_handle,
                                      AudioStreamId p_stream_slot_id);
        void unmap_stream_handle_locked(AudioBusHandle p_audio_bus_handle, AudioBusSlotHandle p_bus_slot_handle);
        AudioStreamId find_stream_slot_id_by_bus_slot_handle_locked(AudioBusHandle p_audio_bus_handle,
                                                                     AudioBusSlotHandle p_bus_slot_handle) const;
        bool bus_has_children_locked(AudioBusHandle p_audio_bus_handle) const;
        AudioBusHandle get_master_bus_handle() const;
        AudioBusSlotHandle get_bus_slot_handle_locked(AudioPlaybackHandle p_audio_playback_handle) const;
        BusSlot *get_bus_slot_locked(AudioBusHandle p_audio_bus_handle);
        const BusSlot *get_bus_slot_locked(AudioBusHandle p_audio_bus_handle) const;
        PlaybackSlot *get_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle);
        const PlaybackSlot *get_playback_slot_locked(AudioPlaybackHandle p_audio_playback_handle) const;
        StreamSlot *get_stream_slot_locked(AudioStreamHandle p_audio_stream_handle);
        const StreamSlot *get_stream_slot_locked(AudioStreamHandle p_audio_stream_handle) const;

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        RenderResult mix_into(AudioBlockView p_block,
                              const MixGainVector &p_upstream_gain) override;

        AudioBusHandle master_bus() const;

        AudioBusHandle create_bus(AudioBusHandle p_parent_bus_handle = InvalidAudioBusHandle);

        void destroy_bus(AudioBusHandle p_audio_bus_handle);

        void play(AudioPlaybackHandle p_audio_playback_handle);

        void pause(AudioPlaybackHandle p_audio_playback_handle);

        void resume(AudioPlaybackHandle p_audio_playback_handle);

        void stop(AudioPlaybackHandle p_audio_playback_handle);

        AudioAssetHandle add_audio(const std::string &p_path, Error &error);

        AudioAssetHandle add_audio(std::unique_ptr<AudioData> p_audio_data, Error &error);

        void remove_audio(AudioAssetHandle p_audio_asset_handle);

        AudioPlaybackHandle create_playback(AudioAssetHandle p_audio_asset_handle);
        AudioPlaybackHandle create_playback(AudioAssetHandle p_audio_asset_handle, AudioBusHandle p_audio_bus_handle);

        AudioPlaybackHandle play_clip(AudioAssetHandle p_audio_asset_handle,
                                      AudioBusHandle p_audio_bus_handle = InvalidAudioBusHandle);

        void destroy_playback(AudioPlaybackHandle p_audio_playback_handle);

        AudioStreamHandle create_stream(AudioBusHandle p_audio_bus_handle = InvalidAudioBusHandle,
                                        size_t p_ring_buffer_size = AudioStream::DEFAULT_STREAM_SIZE);

        void destroy_stream(AudioStreamHandle p_audio_stream_handle);

        size_l write_stream_interleaved(AudioStreamHandle p_audio_stream_handle,
                                        const Sample *p_interleaved,
                                        size_t p_frame_count);

        size_l write_stream_planar(AudioStreamHandle p_audio_stream_handle,
                                   const std::vector<const Sample *> &p_channels,
                                   size_t p_frame_count);

        void set_volume(AudioStreamHandle p_audio_stream_handle, Volume p_volume);

        void set_panning(AudioStreamHandle p_audio_stream_handle, Panning p_panning);

        void play(AudioStreamHandle p_audio_stream_handle);

        void pause(AudioStreamHandle p_audio_stream_handle);

        std::map<AudioAssetId, std::string> get_name_mapping() const;

        void clear_all_audio();

        void stop_all_audio();

        virtual size_l get_frames_remaining(AudioPlaybackHandle p_audio_playback_handle) const;

        virtual size_l get_frame_count(AudioPlaybackHandle p_audio_playback_handle) const;

        virtual size_l get_frame_position(AudioPlaybackHandle p_audio_playback_handle) const;

        void set_volume(AudioPlaybackHandle p_audio_playback_handle, Volume p_volume);
        void set_volume(AudioBusHandle p_audio_bus_handle, Volume p_volume);

        void set_panning(AudioPlaybackHandle p_audio_playback_handle, Panning p_panning);
        void set_panning(AudioBusHandle p_audio_bus_handle, Panning p_panning);

        void reset(AudioPlaybackHandle p_audio_playback_handle);

        void seek_time(AudioPlaybackHandle p_audio_playback_handle, double_l p_seconds);

        void seek_frame(AudioPlaybackHandle p_audio_playback_handle, size_t p_frame);

        AudioSpace(SampleRate p_sample_rate,
                   ChannelLayout p_channel_layout);

        ~AudioSpace() override;
    };
} // namespace Lowl::Audio

#endif
