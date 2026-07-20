#ifndef LOWL_AUDIO_BUS_H
#define LOWL_AUDIO_BUS_H

#include <array>
#include <vector>

#include "audio/lowl_audio_lock_free_queue.h"
#include "audio/source/lowl_audio_bus_event.h"
#include "audio/source/lowl_audio_bus_slot_handle.h"
#include "audio/source/lowl_audio_source.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Single-owner bus node for internal use within AudioSpace.
     *
     * Unlike AudioMixer which supports multi-owner registration and per-owner ack queues,
     * AudioBus assumes a single implicit owner (the AudioSpace that created it). Handle
     * allocation, release, and ack dequeueing require no owner_id parameter.
     *
     * Control-thread methods (allocate_handle, release_handle, submit, remove, try_dequeue_ack)
     * must be called under the caller's own serialization (AudioSpace::state_mutex).
     * The render-thread entry point is mix_into().
     */
    class AudioBus : public AudioSource {
    private:
        static constexpr size_t MAX_ACTIVE_SOURCES = 256;
        static constexpr size_t EVENT_QUEUE_CAPACITY = 512;
        static constexpr size_t ACK_QUEUE_CAPACITY = 512;
        static constexpr size_t InvalidSourceIndex = MAX_ACTIVE_SOURCES;
        static constexpr AudioPlaybackId InvalidSlotId = 0;
        static constexpr AudioPlaybackId FirstSlotId = 1;
        static constexpr size_l LiveFrameCountSentinel = 1;

        struct ActiveSourceSlot {
            AudioBusSlotHandle handle{};
            AudioSource *source = nullptr;
        };

        struct HandleSlot {
            uint16_l generation = 1;
            bool allocated = false;
        };

        std::array<ActiveSourceSlot, MAX_ACTIVE_SOURCES> sources{};
        std::vector<HandleSlot> handles;
        std::vector<AudioPlaybackId> free_slot_ids;
        AudioPlaybackId next_slot_id = FirstSlotId;
        BoundedMpscQueue<AudioBusEvent, EVENT_QUEUE_CAPACITY> events;
        BoundedSpscQueue<AudioBusAck, ACK_QUEUE_CAPACITY> acks;
        size_t active_source_count = 0;

        size_t find_source_index(AudioBusSlotHandle p_handle) const;
        size_t find_free_source_index() const;
        void add_source(size_t p_source_index, AudioBusSlotHandle p_handle, AudioSource *p_audio_source);
        void remove_source(size_t p_source_index);
        void process_events();
        RenderResult render_mixed_block(AudioBlockView p_block, const MixGainVector &p_upstream_gain);

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        RenderResult mix_into(AudioBlockView p_block,
                              const MixGainVector &p_upstream_gain) override;

        AudioBusSlotHandle allocate_handle();
        void release_handle(AudioBusSlotHandle p_handle);

        void submit(AudioBusSlotHandle p_handle, AudioSource *p_audio_source);
        void remove(AudioBusSlotHandle p_handle);
        void remove(AudioBusSlotHandle p_handle, bool p_acknowledge_removal);

        bool try_dequeue_ack(AudioBusAck &p_ack);

        explicit AudioBus(AudioFormat p_audio_format);

        ~AudioBus() override = default;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_BUS_H
