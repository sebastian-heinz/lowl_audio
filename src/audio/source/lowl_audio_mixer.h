#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "audio/lowl_audio_lock_free_queue.h"
#include "audio/source/lowl_audio_mixer_handle.h"
#include "audio/source/lowl_audio_mixer_event.h"
#include "audio/source/lowl_audio_source.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Mixes multiple active sources into a single renderable output stream.
     *
     * This is a live aggregate source with no finite total frame count. Its frame-count queries
     * therefore use a one-frame sentinel so callers can treat it as renderable without observing
     * `frames_remaining > frame_count`.
     */
    class AudioMixer : public AudioSource {
    private:
        static constexpr size_t MAX_ACTIVE_SOURCES = 1024;
        static constexpr size_t MAX_ACK_OWNERS = 64;
        static constexpr size_t EVENT_QUEUE_CAPACITY = MAX_ACTIVE_SOURCES * 4;
        static constexpr size_t ACK_QUEUE_CAPACITY = EVENT_QUEUE_CAPACITY * 2;
        static constexpr size_t InvalidSourceIndex = MAX_ACTIVE_SOURCES;
        static constexpr AudioPlaybackId InvalidHandleId = 0;
        static constexpr AudioPlaybackId FirstHandleId = 1;
        static constexpr size_l LiveFrameCountSentinel = 1;

        struct ActiveSourceSlot {
            AudioMixerHandle handle{};
            AudioSource *source = nullptr;
        };

        struct HandleSlot {
            uint16_l generation = 1;
            AudioSource *bound_source = nullptr;
            bool allocated = false;
        };

        struct AckOwnerSlot {
            std::atomic<bool> registered{false};
            BoundedMpscQueue<AudioMixerAck, ACK_QUEUE_CAPACITY> acknowledgements{};
            std::atomic<bool> queued_ack_overflow{false};
            std::vector<HandleSlot> handles;
            std::vector<AudioPlaybackId> free_handle_ids;
            AudioPlaybackId next_handle_id = FirstHandleId;
        };

        std::array<ActiveSourceSlot, MAX_ACTIVE_SOURCES> sources{};
        std::unique_ptr<AckOwnerSlot[]> ack_owners;
        BoundedMpscQueue<AudioMixerEvent, EVENT_QUEUE_CAPACITY> events{};
        std::mutex ack_owner_mutex;
        AudioBuffer scratch_buffer;
        size_t active_source_count = 0;

        size_t find_source_index(AudioMixerHandle p_handle) const;
        size_t find_free_source_index() const;
        HandleSlot *get_handle_slot_locked(AudioMixerHandle p_handle);
        void add_source(size_t p_source_index, AudioMixerHandle p_handle, AudioSource *p_audio_source);
        void remove_source(size_t p_source_index);
        void process_events();
        RenderResult render_mixed_block(AudioBlockView p_block);
        RenderResult render_chunked_block(AudioBlockView p_block, uint32_t p_chunk_frame_count);
        void enqueue_ack(const AudioMixerAck &p_ack);
        void clear_pending_acks_locked(AckOwnerSlot &p_owner_slot);
        void reset_owner_handles_locked(AckOwnerSlot &p_owner_slot);

    public:
        static constexpr uint32_t DefaultScratchBufferCapacity = 16384;

        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        /**
         * mixes a block from all sources
         */
        RenderResult render(AudioBlockView p_block) override;

        /**
         * adds a audio source to mix
         * Caller must keep p_audio_source alive until a matching terminal acknowledgement
         * (`Removed`, `Finished`, or `Rejected`) is dequeued for p_handle.
         * A handle is bound to exactly one source object for its lifetime and may not
         * be rebound to a different source before `release_handle()`.
         */
        virtual void mix(AudioMixerHandle p_handle, AudioSource *p_audio_source);

        /**
         * removes a audio source from the mix
         * `remove(p_handle, true)` starts asynchronous retirement. The caller may only
         * destroy the source after dequeuing the matching terminal acknowledgement.
         */
        virtual void remove(AudioMixerHandle p_handle);
        virtual void remove(AudioMixerHandle p_handle, bool p_acknowledge_removal);

        uint16_l register_ack_owner();
        /**
         * Owners must unregister only after all of their handles have received a
         * terminal acknowledgement and been released.
         */
        void unregister_ack_owner(uint16_l p_owner_id);
        AudioMixerHandle allocate_handle(uint16_l p_owner_id);
        /**
         * Releases a caller-owned handle after the matching source has been retired
         * and its terminal acknowledgement has been observed.
         */
        void release_handle(AudioMixerHandle p_handle);
        bool try_dequeue_ack(uint16_l p_owner_id, AudioMixerAck &p_ack);

        AudioMixer(SampleRate p_sample_rate,
                   ChannelLayout p_channel_layout,
                   uint32_t p_scratch_buffer_capacity = DefaultScratchBufferCapacity);

        ~AudioMixer() override = default;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_MIXER_H
