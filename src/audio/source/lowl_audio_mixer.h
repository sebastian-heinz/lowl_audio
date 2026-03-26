#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include <concurrentqueue.h>

#include <array>
#include <atomic>
#include <mutex>

#include "audio/source/lowl_audio_mixer_handle.h"
#include "audio/source/lowl_audio_mixer_event.h"
#include "audio/source/lowl_audio_source.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Mixes multiple active sources into a single renderable output stream.
     */
    class AudioMixer : public AudioSource {
    private:
        static constexpr uint32_t SCRATCH_BUFFER_CAPACITY = 8192;
        static constexpr size_t MAX_ACTIVE_SOURCES = 1024;
        static constexpr size_t MAX_ACK_OWNERS = 64;
        static constexpr size_t InvalidSourceIndex = MAX_ACTIVE_SOURCES;

        struct ActiveSourceSlot {
            AudioMixerHandle handle{};
            AudioSource *source = nullptr;
        };

        struct AckOwnerSlot {
            std::atomic<bool> registered{false};
            std::unique_ptr<moodycamel::ConcurrentQueue<AudioMixerAck>> acknowledgements;
        };

        std::array<ActiveSourceSlot, MAX_ACTIVE_SOURCES> sources{};
        std::array<AckOwnerSlot, MAX_ACK_OWNERS> ack_owners{};
        std::unique_ptr<moodycamel::ConcurrentQueue<AudioMixerEvent>> events;
        std::mutex ack_owner_mutex;
        AudioBuffer scratch_buffer;

        size_t find_source_index(AudioMixerHandle p_handle) const;
        size_t find_source_index(const AudioSource *p_audio_source) const;
        size_t find_free_source_index() const;
        void enqueue_ack(const AudioMixerAck &p_ack);
        void clear_ack_queue(AckOwnerSlot &p_owner_slot);

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        /**
         * mixes a block from all sources
         */
        RenderResult render(AudioBlockView p_block) override;

        /**
         * adds a audio source to mix
         */
        virtual void mix(AudioSource *p_audio_source);
        virtual void mix(AudioMixerHandle p_handle, AudioSource *p_audio_source);

        /**
         * removes a audio source from the mix
         */
        virtual void remove(AudioSource *p_audio_source);
        virtual void remove(AudioMixerHandle p_handle);
        virtual void remove(AudioMixerHandle p_handle, bool p_acknowledge_removal);

        uint16_l register_ack_owner();
        void unregister_ack_owner(uint16_l p_owner_id);
        bool try_dequeue_ack(uint16_l p_owner_id, AudioMixerAck &p_ack);

        AudioMixer(SampleRate p_sample_rate, AudioChannel p_channel);

        ~AudioMixer() override = default;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_MIXER_H
