#ifndef LOWL_AUDIO_GRAPH_H
#define LOWL_AUDIO_GRAPH_H

#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

#include "audio/source/lowl_audio_mixer.h"
#include "audio/source/lowl_audio_node_handle.h"
#include "audio/source/lowl_audio_source.h"
#include "lowl_error.h"

namespace Lowl::Audio {
    /**
     * Owning, renderable audio topology.
     *
     * Nodes created or adopted by a graph are owned until destroy() or graph destruction.
     * A permanent internal root mixer makes the complete graph usable anywhere an AudioSource
     * is accepted. Standalone AudioMixer and AudioSpace instances remain independent of this API.
     * Topology is valid when connection lifecycle calls go through AudioGraph. Sources borrowed
     * with get() remain configurable, but callers must not invoke AudioMixer connection lifecycle
     * methods on graph-owned mixers directly.
     * Exactly one control thread may mutate a graph, and exactly one render thread may render it.
     */
    class AudioGraph : public AudioSource {
    public:
        static constexpr size_t MaxNodes = 1024;
        static constexpr size_t MaxTopologyDepth = 64;

    private:
        static constexpr AudioNodeId RootNodeId = 1;
        static constexpr size_l LiveFrameCountSentinel = 1;

        static_assert(MaxNodes <= std::numeric_limits<AudioNodeId>::max(),
                      "AudioNodeId must represent every active graph node");

        struct Node {
            AudioNodeId id = 0;
            std::vector<std::unique_ptr<Node>> children;
            std::unique_ptr<AudioSource> source;
            AudioMixerHandle mixer_connection{};
            bool disconnect_pending = false;
            bool destroy_when_disconnected = false;

            explicit Node(AudioNodeId p_id)
                : id(p_id) {
            }
        };

        struct NodeLocation {
            Node *node = nullptr;
            Node *parent = nullptr;
            size_t depth = 0;
            bool render_reachable = false;
            bool topology_retiring = false;
            bool destruction_pending = false;
        };

        std::unique_ptr<Node> render_root;
        std::vector<std::unique_ptr<Node>> detached_nodes;
        mutable std::mutex control_mutex;
        AudioInstanceId graph_id = 0;
        AudioNodeId next_node_id = RootNodeId + 1;
        size_t node_count = 1;
        bool shutting_down = false;

        static bool find_node_in_subtree(Node &p_node,
                                         AudioNodeId p_node_id,
                                         Node *p_parent,
                                         size_t p_depth,
                                         bool p_render_reachable,
                                         bool p_ancestor_retiring,
                                         bool p_ancestor_destruction,
                                         NodeLocation &r_location);
        static size_t get_subtree_depth(const Node &p_node);
        static size_t get_subtree_size(const Node &p_node);

        NodeLocation find_node_locked(AudioNodeHandle p_handle, Error &p_error, const char *p_operation);
        NodeLocation find_node_locked(AudioNodeHandle p_handle);
        size_t find_detached_root_index_locked(AudioNodeId p_node_id) const;
        void collect_mixer_completions_locked(Node &p_node, size_t &p_remaining_completions);
        void update_locked();
        void shutdown_subtree_quiescent(Node &p_node);
        void report_node_type_mismatch(Error &p_error) const;

    public:
        explicit AudioGraph(AudioFormat p_audio_format);
        ~AudioGraph() override;

        AudioGraph(const AudioGraph &) = delete;
        AudioGraph &operator=(const AudioGraph &) = delete;
        AudioGraph(AudioGraph &&) = delete;
        AudioGraph &operator=(AudioGraph &&) = delete;

        [[nodiscard]] AudioNodeHandle root() const;

        /**
         * Transfers p_source only after all validation succeeds; failures leave it unchanged.
         * The caller must first remove any standalone mixer connections to this source.
         */
        [[nodiscard]] AudioNodeHandle add(std::unique_ptr<AudioSource> &p_source, Error &p_error);

        template <typename T>
        [[nodiscard]] AudioNodeHandle add(std::unique_ptr<T> &p_source, Error &p_error) {
            static_assert(std::is_base_of_v<AudioSource, T>, "AudioGraph nodes must derive from AudioSource");
            std::unique_ptr<AudioSource> source = std::move(p_source);
            const AudioNodeHandle handle = add(source, p_error);
            if (!handle.is_valid()) {
                p_source.reset(static_cast<T *>(source.release()));
            }
            return handle;
        }

        template <typename T, typename... Args>
        [[nodiscard]] AudioNodeHandle create(Error &p_error, Args &&...p_args) {
            static_assert(std::is_base_of_v<AudioSource, T>, "AudioGraph nodes must derive from AudioSource");
            std::unique_ptr<AudioSource> source = std::make_unique<T>(std::forward<Args>(p_args)...);
            return add(source, p_error);
        }

        /** Borrowed pointer remains valid until its node or an owning subtree is destroyed. */
        AudioSource *get_source(AudioNodeHandle p_handle, Error &p_error);
        const AudioSource *get_source(AudioNodeHandle p_handle, Error &p_error) const;

        template <typename T>
        T *get(AudioNodeHandle p_handle, Error &p_error) {
            static_assert(std::is_base_of_v<AudioSource, T>, "Requested graph type must derive from AudioSource");
            AudioSource *source = get_source(p_handle, p_error);
            if (!source) {
                return nullptr;
            }
            T *typed_source = dynamic_cast<T *>(source);
            if (!typed_source) {
                report_node_type_mismatch(p_error);
            }
            return typed_source;
        }

        template <typename T>
        const T *get(AudioNodeHandle p_handle, Error &p_error) const {
            static_assert(std::is_base_of_v<AudioSource, T>, "Requested graph type must derive from AudioSource");
            const AudioSource *source = get_source(p_handle, p_error);
            if (!source) {
                return nullptr;
            }
            const T *typed_source = dynamic_cast<const T *>(source);
            if (!typed_source) {
                report_node_type_mismatch(p_error);
            }
            return typed_source;
        }

        /** Attaches a detached subtree root to a render-reachable mixer. Build trees from root downward. */
        void connect(AudioNodeHandle p_parent, AudioNodeHandle p_child, Error &p_error);

        /** Requests detachment while preserving the complete subtree. update() finalizes the move. */
        void disconnect(AudioNodeHandle p_node, Error &p_error);

        /** Destroys a detached subtree immediately or an active subtree after one render acknowledgement. */
        void destroy(AudioNodeHandle p_node, Error &p_error);

        /** Collects a bounded number of render acknowledgements and finalizes lifecycle changes. */
        void update();

        bool contains(AudioNodeHandle p_handle) const;
        bool is_connected(AudioNodeHandle p_handle, Error &p_error) const;
        bool is_retiring(AudioNodeHandle p_handle, Error &p_error) const;
        size_t get_node_count() const;

        RenderResult mix_into(AudioBlockView p_block,
                              const MixGainVector &p_upstream_gain) override;
        size_l get_frames_remaining() const override;
        size_l get_frame_position() const override;
        size_l get_frame_count() const override;
    };
} // namespace Lowl::Audio

#endif // LOWL_AUDIO_GRAPH_H
