#include "lowl_audio_graph.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <string>

#include "lowl_logger.h"

namespace {
    std::atomic<Lowl::AudioInstanceId> next_audio_graph_id{1};

    Lowl::AudioInstanceId allocate_audio_graph_id() {
        const Lowl::AudioInstanceId graph_id = next_audio_graph_id.fetch_add(1, std::memory_order_relaxed);
        if (graph_id == 0 || graph_id == std::numeric_limits<Lowl::AudioInstanceId>::max()) {
            LOWL_LOG_ERROR("AudioGraph: process-wide graph identity capacity is exhausted.");
            std::abort();
        }
        return graph_id;
    }
} // namespace

Lowl::Audio::AudioGraph::AudioGraph(const AudioFormat p_audio_format)
    : AudioSource(p_audio_format),
      graph_id(allocate_audio_graph_id()) {
    detached_nodes.reserve(MaxNodes - 1);

    auto root_mixer = std::make_unique<AudioMixer>(p_audio_format);
    auto root_node = std::make_unique<Node>(RootNodeId);
    root_node->source = std::move(root_mixer);
    render_root = std::move(root_node);
}

Lowl::Audio::AudioGraph::~AudioGraph() {
    std::lock_guard<std::mutex> lock(control_mutex);
    shutting_down = true;

    if (render_root) {
        shutdown_subtree_quiescent(*render_root);
    }
    for (const std::unique_ptr<Node> &node : detached_nodes) {
        shutdown_subtree_quiescent(*node);
    }

    detached_nodes.clear();
    render_root.reset();
    node_count = 0;
}

bool Lowl::Audio::AudioGraph::find_node_in_subtree(Node &p_node,
                                                    const AudioNodeId p_node_id,
                                                    Node *p_parent,
                                                    const size_t p_depth,
                                                    const bool p_render_reachable,
                                                    const bool p_ancestor_retiring,
                                                    const bool p_ancestor_destruction,
                                                    NodeLocation &r_location) {
    const bool topology_retiring = p_ancestor_retiring || p_node.disconnect_pending;
    const bool destruction_pending = p_ancestor_destruction || p_node.destroy_when_disconnected;
    if (p_node.id == p_node_id) {
        r_location.node = &p_node;
        r_location.parent = p_parent;
        r_location.depth = p_depth;
        r_location.render_reachable = p_render_reachable;
        r_location.topology_retiring = topology_retiring;
        r_location.destruction_pending = destruction_pending;
        return true;
    }

    for (const std::unique_ptr<Node> &child : p_node.children) {
        if (find_node_in_subtree(*child,
                                 p_node_id,
                                 &p_node,
                                 p_depth + 1,
                                 p_render_reachable,
                                 topology_retiring,
                                 destruction_pending,
                                 r_location)) {
            return true;
        }
    }
    return false;
}

size_t Lowl::Audio::AudioGraph::get_subtree_depth(const Node &p_node) {
    size_t depth = 1;
    for (const std::unique_ptr<Node> &child : p_node.children) {
        depth = std::max(depth, static_cast<size_t>(1 + get_subtree_depth(*child)));
    }
    return depth;
}

size_t Lowl::Audio::AudioGraph::get_subtree_size(const Node &p_node) {
    size_t size = 1;
    for (const std::unique_ptr<Node> &child : p_node.children) {
        size += get_subtree_size(*child);
    }
    return size;
}

Lowl::Audio::AudioGraph::NodeLocation
Lowl::Audio::AudioGraph::find_node_locked(const AudioNodeHandle p_handle) {
    NodeLocation location{};
    if (!p_handle.is_valid() || p_handle.graph_id != graph_id || !render_root) {
        return location;
    }

    if (find_node_in_subtree(*render_root, p_handle.node_id, nullptr, 1, true, false, false, location)) {
        return location;
    }
    for (const std::unique_ptr<Node> &node : detached_nodes) {
        if (find_node_in_subtree(*node,
                                 p_handle.node_id,
                                 nullptr,
                                 1,
                                 false,
                                 false,
                                 false,
                                 location)) {
            return location;
        }
    }
    return {};
}

Lowl::Audio::AudioGraph::NodeLocation
Lowl::Audio::AudioGraph::find_node_locked(const AudioNodeHandle p_handle,
                                           Error &p_error,
                                           const char *p_operation) {
    if (!p_handle.is_valid() || p_handle.graph_id != graph_id) {
        LOWL_LOG_ERROR(std::string(p_operation) + ": node handle is invalid or belongs to another graph.");
        p_error.set_error(ErrorCode::GraphNodeInvalid);
        return {};
    }

    NodeLocation location = find_node_locked(p_handle);
    if (!location.node) {
        LOWL_LOG_ERROR(std::string(p_operation) + ": node handle is stale or unknown.");
        p_error.set_error(ErrorCode::GraphNodeInvalid);
    }
    return location;
}

size_t Lowl::Audio::AudioGraph::find_detached_root_index_locked(const AudioNodeId p_node_id) const {
    for (size_t index = 0; index < detached_nodes.size(); index++) {
        if (detached_nodes[index]->id == p_node_id) {
            return index;
        }
    }
    return detached_nodes.size();
}

void Lowl::Audio::AudioGraph::shutdown_subtree_quiescent(Node &p_node) {
    if (auto *mixer = dynamic_cast<AudioMixer *>(p_node.source.get())) {
        mixer->shutdown_quiescent();
    }
    for (const std::unique_ptr<Node> &child : p_node.children) {
        shutdown_subtree_quiescent(*child);
    }
}

void Lowl::Audio::AudioGraph::collect_mixer_completions_locked(Node &p_node,
                                                               size_t &p_remaining_completions) {
    if (p_remaining_completions == 0) {
        return;
    }

    if (auto *mixer = dynamic_cast<AudioMixer *>(p_node.source.get())) {
        AudioMixerCompletion completion{};
        while (p_remaining_completions > 0 && mixer->try_collect_completion(completion)) {
            p_remaining_completions--;

            size_t child_index = p_node.children.size();
            for (size_t index = 0; index < p_node.children.size(); index++) {
                if (p_node.children[index]->mixer_connection == completion.handle) {
                    child_index = index;
                    break;
                }
            }
            if (child_index == p_node.children.size()) {
                LOWL_LOG_ERROR("AudioGraph::update: mixer completion does not match an owned child node.");
                assert(false && "Graph-managed mixer completion must match graph ownership");
                continue;
            }

            std::unique_ptr<Node> completed_node = std::move(p_node.children[child_index]);
            p_node.children.erase(p_node.children.begin() + static_cast<std::ptrdiff_t>(child_index));
            completed_node->mixer_connection = {};
            completed_node->disconnect_pending = false;

            if (completed_node->destroy_when_disconnected) {
                const size_t destroyed_node_count = get_subtree_size(*completed_node);
                shutdown_subtree_quiescent(*completed_node);
                node_count -= destroyed_node_count;
                continue;
            }

            detached_nodes.push_back(std::move(completed_node));
        }
    }

    for (const std::unique_ptr<Node> &child : p_node.children) {
        collect_mixer_completions_locked(*child, p_remaining_completions);
        if (p_remaining_completions == 0) {
            return;
        }
    }
}

void Lowl::Audio::AudioGraph::update_locked() {
    if (shutting_down || !render_root) {
        return;
    }

    size_t remaining_completions = MaxNodes;
    collect_mixer_completions_locked(*render_root, remaining_completions);
    for (size_t root_index = 0;
         root_index < detached_nodes.size() && remaining_completions > 0;
         root_index++) {
        collect_mixer_completions_locked(*detached_nodes[root_index], remaining_completions);
    }
}

void Lowl::Audio::AudioGraph::report_node_type_mismatch(Error &p_error) const {
    LOWL_LOG_ERROR("AudioGraph::get: node does not have the requested audio source type.");
    p_error.set_error(ErrorCode::GraphNodeTypeMismatch);
}

Lowl::AudioNodeHandle Lowl::Audio::AudioGraph::root() const {
    return {graph_id, RootNodeId};
}

Lowl::AudioNodeHandle Lowl::Audio::AudioGraph::add(std::unique_ptr<AudioSource> &p_source, Error &p_error) {
    p_error.clear();
    if (!p_source) {
        LOWL_LOG_ERROR("AudioGraph::add: source must not be null.");
        p_error.set_error(ErrorCode::InvalidParameter);
        return {};
    }
    if (p_source->get_audio_format() != get_audio_format()) {
        LOWL_LOG_ERROR("AudioGraph::add: source format must match the graph format.");
        p_error.set_error(ErrorCode::UnsupportedAudioFormat);
        return {};
    }
    std::lock_guard<std::mutex> lock(control_mutex);
    update_locked();
    if (shutting_down) {
        LOWL_LOG_ERROR("AudioGraph::add: graph is shutting down.");
        p_error.set_error(ErrorCode::GraphShutdown);
        return {};
    }
    if (node_count >= MaxNodes) {
        LOWL_LOG_ERROR("AudioGraph::add: fixed node capacity is exhausted.");
        p_error.set_error(ErrorCode::GraphNodeCapacityExhausted);
        return {};
    }
    if (next_node_id == 0 || next_node_id == std::numeric_limits<AudioNodeId>::max()) {
        LOWL_LOG_ERROR("AudioGraph::add: graph node identity capacity is exhausted.");
        p_error.set_error(ErrorCode::GraphNodeIdentityExhausted);
        return {};
    }

    const AudioNodeId node_id = next_node_id;
    auto node = std::make_unique<Node>(node_id);
    node->source = std::move(p_source);
    next_node_id++;
    detached_nodes.push_back(std::move(node));
    node_count++;
    return {graph_id, node_id};
}

Lowl::Audio::AudioSource *Lowl::Audio::AudioGraph::get_source(const AudioNodeHandle p_handle, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(control_mutex);
    NodeLocation location = find_node_locked(p_handle, p_error, "AudioGraph::get_source");
    if (!location.node) {
        return nullptr;
    }
    if (location.destruction_pending) {
        LOWL_LOG_ERROR("AudioGraph::get_source: node or one of its owners is retiring.");
        p_error.set_error(ErrorCode::GraphNodeRetiring);
        return nullptr;
    }
    return location.node->source.get();
}

const Lowl::Audio::AudioSource *
Lowl::Audio::AudioGraph::get_source(const AudioNodeHandle p_handle, Error &p_error) const {
    p_error.clear();
    std::lock_guard<std::mutex> lock(control_mutex);
    AudioGraph *self = const_cast<AudioGraph *>(this);
    NodeLocation location = self->find_node_locked(p_handle, p_error, "AudioGraph::get_source");
    if (!location.node) {
        return nullptr;
    }
    if (location.destruction_pending) {
        LOWL_LOG_ERROR("AudioGraph::get_source: node or one of its owners is retiring.");
        p_error.set_error(ErrorCode::GraphNodeRetiring);
        return nullptr;
    }
    return location.node->source.get();
}

void Lowl::Audio::AudioGraph::connect(const AudioNodeHandle p_parent,
                                      const AudioNodeHandle p_child,
                                      Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(control_mutex);
    update_locked();
    if (shutting_down) {
        LOWL_LOG_ERROR("AudioGraph::connect: graph is shutting down.");
        p_error.set_error(ErrorCode::GraphShutdown);
        return;
    }

    NodeLocation parent = find_node_locked(p_parent, p_error, "AudioGraph::connect");
    if (!parent.node) {
        return;
    }
    NodeLocation child = find_node_locked(p_child, p_error, "AudioGraph::connect");
    if (!child.node) {
        return;
    }
    if (!parent.render_reachable) {
        LOWL_LOG_ERROR("AudioGraph::connect: parent must be reachable from the graph root.");
        p_error.set_error(ErrorCode::GraphParentNotReachable);
        return;
    }
    if (parent.topology_retiring || parent.destruction_pending) {
        LOWL_LOG_ERROR("AudioGraph::connect: parent or one of its owners is retiring.");
        p_error.set_error(ErrorCode::GraphNodeRetiring);
        return;
    }

    const size_t detached_index = find_detached_root_index_locked(child.node->id);
    if (detached_index == detached_nodes.size()) {
        LOWL_LOG_ERROR("AudioGraph::connect: child is already connected or is not a detached subtree root.");
        p_error.set_error(ErrorCode::GraphNodeAlreadyConnected);
        return;
    }
    if (child.topology_retiring || child.destruction_pending) {
        LOWL_LOG_ERROR("AudioGraph::connect: child subtree is retiring.");
        p_error.set_error(ErrorCode::GraphNodeRetiring);
        return;
    }

    auto *parent_mixer = dynamic_cast<AudioMixer *>(parent.node->source.get());
    if (!parent_mixer) {
        LOWL_LOG_ERROR("AudioGraph::connect: parent node is not an AudioMixer.");
        p_error.set_error(ErrorCode::GraphParentNotMixer);
        return;
    }
    if (parent.depth + get_subtree_depth(*child.node) > MaxTopologyDepth) {
        LOWL_LOG_ERROR("AudioGraph::connect: fixed topology depth is exceeded.");
        p_error.set_error(ErrorCode::GraphTopologyDepthExceeded);
        return;
    }

    parent.node->children.reserve(parent.node->children.size() + 1);
    const AudioMixerHandle connection = parent_mixer->connect(*child.node->source, p_error);
    if (p_error.has_error() || !connection.is_valid()) {
        LOWL_LOG_ERROR("AudioGraph::connect: parent mixer rejected the child source.");
        return;
    }

    std::unique_ptr<Node> connected_node = std::move(detached_nodes[detached_index]);
    detached_nodes.erase(detached_nodes.begin() + static_cast<std::ptrdiff_t>(detached_index));
    connected_node->mixer_connection = connection;
    parent.node->children.push_back(std::move(connected_node));
}

void Lowl::Audio::AudioGraph::disconnect(const AudioNodeHandle p_node, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(control_mutex);
    update_locked();
    if (shutting_down) {
        LOWL_LOG_ERROR("AudioGraph::disconnect: graph is shutting down.");
        p_error.set_error(ErrorCode::GraphShutdown);
        return;
    }

    NodeLocation location = find_node_locked(p_node, p_error, "AudioGraph::disconnect");
    if (!location.node) {
        return;
    }
    if (location.node == render_root.get()) {
        LOWL_LOG_ERROR("AudioGraph::disconnect: the permanent graph root cannot be disconnected.");
        p_error.set_error(ErrorCode::GraphRootOperationInvalid);
        return;
    }
    if (location.topology_retiring || location.destruction_pending) {
        LOWL_LOG_ERROR("AudioGraph::disconnect: node or one of its owners is already retiring.");
        p_error.set_error(ErrorCode::GraphNodeRetiring);
        return;
    }
    if (!location.render_reachable || !location.parent || !location.node->mixer_connection.is_valid()) {
        LOWL_LOG_ERROR("AudioGraph::disconnect: node is not connected to the active render tree.");
        p_error.set_error(ErrorCode::GraphNodeNotConnected);
        return;
    }

    auto *parent_mixer = dynamic_cast<AudioMixer *>(location.parent->source.get());
    assert(parent_mixer && "Only mixers may own graph children");
    if (!parent_mixer) {
        p_error.set_error(ErrorCode::GraphParentNotMixer);
        return;
    }
    parent_mixer->disconnect(location.node->mixer_connection, p_error);
    if (p_error.has_error()) {
        LOWL_LOG_ERROR("AudioGraph::disconnect: parent mixer rejected the disconnection request.");
        return;
    }
    location.node->disconnect_pending = true;
}

void Lowl::Audio::AudioGraph::destroy(const AudioNodeHandle p_node, Error &p_error) {
    p_error.clear();
    std::lock_guard<std::mutex> lock(control_mutex);
    update_locked();
    if (shutting_down) {
        LOWL_LOG_ERROR("AudioGraph::destroy: graph is shutting down.");
        p_error.set_error(ErrorCode::GraphShutdown);
        return;
    }

    NodeLocation location = find_node_locked(p_node, p_error, "AudioGraph::destroy");
    if (!location.node) {
        return;
    }
    if (location.node == render_root.get()) {
        LOWL_LOG_ERROR("AudioGraph::destroy: the permanent graph root cannot be destroyed.");
        p_error.set_error(ErrorCode::GraphRootOperationInvalid);
        return;
    }
    if (location.destruction_pending) {
        LOWL_LOG_ERROR("AudioGraph::destroy: node or one of its owners is already being destroyed.");
        p_error.set_error(ErrorCode::GraphNodeRetiring);
        return;
    }
    if (location.node->disconnect_pending) {
        location.node->destroy_when_disconnected = true;
        return;
    }
    if (location.topology_retiring) {
        LOWL_LOG_ERROR("AudioGraph::destroy: an owning subtree is currently disconnecting.");
        p_error.set_error(ErrorCode::GraphNodeRetiring);
        return;
    }

    const size_t detached_index = find_detached_root_index_locked(location.node->id);
    if (detached_index != detached_nodes.size()) {
        const size_t destroyed_node_count = get_subtree_size(*location.node);
        shutdown_subtree_quiescent(*location.node);
        detached_nodes.erase(detached_nodes.begin() + static_cast<std::ptrdiff_t>(detached_index));
        node_count -= destroyed_node_count;
        return;
    }
    if (!location.render_reachable || !location.parent || !location.node->mixer_connection.is_valid()) {
        LOWL_LOG_ERROR(
            "AudioGraph::destroy: only an active node or the root of a detached subtree can be destroyed.");
        p_error.set_error(ErrorCode::GraphParentNotReachable);
        return;
    }

    auto *parent_mixer = dynamic_cast<AudioMixer *>(location.parent->source.get());
    assert(parent_mixer && "Only mixers may own graph children");
    if (!parent_mixer) {
        p_error.set_error(ErrorCode::GraphParentNotMixer);
        return;
    }
    parent_mixer->disconnect(location.node->mixer_connection, p_error);
    if (p_error.has_error()) {
        LOWL_LOG_ERROR("AudioGraph::destroy: parent mixer rejected the disconnection request.");
        return;
    }
    location.node->disconnect_pending = true;
    location.node->destroy_when_disconnected = true;
}

void Lowl::Audio::AudioGraph::update() {
    std::lock_guard<std::mutex> lock(control_mutex);
    update_locked();
}

bool Lowl::Audio::AudioGraph::contains(const AudioNodeHandle p_handle) const {
    std::lock_guard<std::mutex> lock(control_mutex);
    return const_cast<AudioGraph *>(this)->find_node_locked(p_handle).node != nullptr;
}

bool Lowl::Audio::AudioGraph::is_connected(const AudioNodeHandle p_handle, Error &p_error) const {
    p_error.clear();
    std::lock_guard<std::mutex> lock(control_mutex);
    AudioGraph *self = const_cast<AudioGraph *>(this);
    NodeLocation location = self->find_node_locked(p_handle, p_error, "AudioGraph::is_connected");
    if (!location.node) {
        return false;
    }
    return location.parent != nullptr;
}

bool Lowl::Audio::AudioGraph::is_retiring(const AudioNodeHandle p_handle, Error &p_error) const {
    p_error.clear();
    std::lock_guard<std::mutex> lock(control_mutex);
    AudioGraph *self = const_cast<AudioGraph *>(this);
    NodeLocation location = self->find_node_locked(p_handle, p_error, "AudioGraph::is_retiring");
    if (!location.node) {
        return false;
    }
    return location.topology_retiring || location.destruction_pending;
}

size_t Lowl::Audio::AudioGraph::get_node_count() const {
    std::lock_guard<std::mutex> lock(control_mutex);
    return node_count;
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioGraph::mix_into(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    auto *root_mixer = static_cast<AudioMixer *>(render_root->source.get());
    const bool graph_is_playing = playback_enabled.load(std::memory_order_acquire);
    const bool format_matches = p_block.channel_count == get_channel_count();
    if (!graph_is_playing || !format_matches) {
        AudioBlockView control_block{};
        control_block.channel_count = get_channel_count();
        root_mixer->mix_into(control_block, compose_gain_vector(p_upstream_gain));
        return {0, graph_is_playing ? RenderState::Error : RenderState::Starved};
    }
    return root_mixer->mix_into(p_block, compose_gain_vector(p_upstream_gain));
}

Lowl::size_l Lowl::Audio::AudioGraph::get_frames_remaining() const {
    return LiveFrameCountSentinel;
}

Lowl::size_l Lowl::Audio::AudioGraph::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioGraph::get_frame_count() const {
    return LiveFrameCountSentinel;
}
