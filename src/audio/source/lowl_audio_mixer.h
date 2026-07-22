#ifndef LOWL_AUDIO_MIXER_H
#define LOWL_AUDIO_MIXER_H

#include <array>
#include <atomic>
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
     * Exactly one render thread may call mix_into for a mixer and its descendants.
     * AudioMixer deliberately does not own sources or validate topology. Standalone callers must
     * keep connected sources alive, avoid cycles, and prevent concurrent rendering of one stateful
     * source through multiple paths. Use AudioGraph when those invariants should be enforced.
     *
     * This is a live aggregate source with no finite total frame count. Its frame-count queries
     * therefore use a one-frame sentinel so callers can treat it as renderable without observing
     * `frames_remaining > frame_count`.
     */
    class AudioMixer : public AudioSource {
    public:
        static constexpr size_t MaxConnections = 1024;
        static constexpr size_t MaxEventsPerRender = 64;

        static_assert(std::atomic<bool>::is_always_lock_free,
                      "Mixer state flags must be lock-free for real-time audio safety");

    private:
        static constexpr size_t EventQueueCapacity = MaxConnections;
        static constexpr size_t CompletionQueueCapacity = MaxConnections;
        static constexpr size_t InvalidConnectionIndex = MaxConnections;
        static constexpr size_l LiveFrameCountSentinel = 1;

        struct RenderConnectionSlot {
            AudioSource *source = nullptr;
            uint64_l generation = 0;
        };

        struct ControlConnectionSlot {
            AudioSource *source = nullptr;
            uint64_l generation = 1;
        };

        std::array<RenderConnectionSlot, MaxConnections> render_connections{};
        std::array<ControlConnectionSlot, MaxConnections> control_connections{};
        std::array<std::atomic<bool>, MaxConnections> disconnect_requests{};
        BoundedSpscQueue<AudioMixerEvent, EventQueueCapacity> events{};
        BoundedSpscQueue<AudioMixerCompletion, CompletionQueueCapacity> completions{};
        mutable std::mutex control_mutex;
        std::atomic<bool> shut_down{false};
        uint64_l mixer_id;
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
        void process_disconnect_requests();
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
         * is collected. The caller must keep p_audio_source alive until then or until
         * the whole render graph is quiescent and shutdown_quiescent() has returned.
         * This standalone API does not reject cycles or duplicate source connections.
         */
        [[nodiscard]] AudioMixerHandle connect(AudioSource &p_audio_source, Error &p_error);

        /**
         * Requests terminal disconnection. On success, the caller must wait for the
         * matching completion before destroying the source.
         */
        void disconnect(AudioMixerHandle p_handle, Error &p_error);

        /**
         * Collects one terminal connection result and recycles its connection slot.
         */
        [[nodiscard]] bool try_collect_completion(AudioMixerCompletion &p_completion);

        /**
         * Permanently closes the mixer and releases every child connection synchronously.
         * The caller must first guarantee that no render or control call can run concurrently.
         * This is the whole-graph shutdown alternative to collecting individual completions.
         */
        void shutdown_quiescent();

        explicit AudioMixer(AudioFormat p_audio_format);

        ~AudioMixer() override;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_MIXER_H
