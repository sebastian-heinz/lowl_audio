#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include <array>
#include <mutex>

#include "audio/lowl_audio_lock_free_queue.h"
#include "audio/source/lowl_audio_mixer_handle.h"
#include "audio/source/lowl_audio_mixer_event.h"
#include "audio/source/lowl_audio_source.h"
#include "lowl_error.h"
#include "lowl_typedef.h"

namespace Lowl::Audio {
    /**
     * Mixes multiple active sources into a single renderable output stream.
     *
     * One controller owns each Mixer instance. A handle represents one fixed-capacity,
     * one-shot connection lifetime. Capacity is reserved before the connect command is
     * accepted and is released only when its terminal completion is collected.
     *
     * This is a live aggregate source with no finite total frame count. Its frame-count queries
     * therefore use a one-frame sentinel so callers can treat it as renderable without observing
     * `frames_remaining > frame_count`.
     */
    class AudioMixer : public AudioSource {
    private:
        static constexpr size_t MAX_CONNECTIONS = 1024;
        static constexpr size_t EVENT_QUEUE_CAPACITY = MAX_CONNECTIONS * 2;
        static constexpr size_t COMPLETION_QUEUE_CAPACITY = MAX_CONNECTIONS;
        static constexpr size_t InvalidConnectionIndex = MAX_CONNECTIONS;
        static constexpr size_l LiveFrameCountSentinel = 1;

        struct RenderConnectionSlot {
            AudioMixerHandle handle{};
            AudioSource *source = nullptr;
        };

        struct ControlConnectionSlot {
            uint16_l generation = 1;
            bool allocated = false;
            bool disconnect_queued = false;
        };

        std::array<RenderConnectionSlot, MAX_CONNECTIONS> render_connections{};
        std::array<ControlConnectionSlot, MAX_CONNECTIONS> control_connections{};
        BoundedMpscQueue<AudioMixerEvent, EVENT_QUEUE_CAPACITY> events{};
        BoundedSpscQueue<AudioMixerCompletion, COMPLETION_QUEUE_CAPACITY> completions{};
        std::mutex control_mutex;
        uint32_l mixer_id;
        size_t active_source_count = 0;

        static size_t get_connection_index(AudioMixerHandle p_handle);
        size_t find_free_connection_index_locked() const;
        ControlConnectionSlot *get_control_connection_locked(AudioMixerHandle p_handle);
        void connect_source(size_t p_connection_index,
                            AudioMixerHandle p_handle,
                            AudioSource *p_audio_source);
        void disconnect_source(size_t p_connection_index);
        void complete_connection(size_t p_connection_index, AudioMixerCompletion::Type p_type);
        void process_events();
        RenderResult render_mixed_block(AudioBlockView p_block, const MixGainVector &p_upstream_gain);
        void enqueue_completion(const AudioMixerCompletion &p_completion);

    public:
        size_l get_frames_remaining() const override;

        size_l get_frame_position() const override;

        size_l get_frame_count() const override;

        /**
         * mixes a block from all sources
         */
        RenderResult mix_into(AudioBlockView p_block,
                              const MixGainVector &p_upstream_gain) override;

        /**
         * Reserves one connection and queues its source for rendering. The returned
         * handle is one-shot and remains reserved until its terminal completion
         * is collected. The caller must keep p_audio_source alive until then.
         */
        AudioMixerHandle connect(AudioSource *p_audio_source, Error &p_error);

        /**
         * Queues terminal disconnection. On success, the caller must wait for the
         * matching completion before destroying the source.
         */
        void disconnect(AudioMixerHandle p_handle, Error &p_error);

        /**
         * Collects one terminal connection result and recycles its connection slot.
         */
        bool try_collect_completion(AudioMixerCompletion &p_completion);

        explicit AudioMixer(AudioFormat p_audio_format);

        ~AudioMixer() override = default;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_MIXER_H
