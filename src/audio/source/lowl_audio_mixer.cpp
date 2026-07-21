#include "lowl_audio_mixer.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <limits>
#include <string>

#include "lowl_logger.h"

namespace {
    std::atomic<Lowl::uint32_l> next_audio_mixer_id{1};

    Lowl::uint16_l advance_generation(const Lowl::uint16_l p_current) {
        return p_current == std::numeric_limits<Lowl::uint16_l>::max()
                   ? 1
                   : static_cast<Lowl::uint16_l>(p_current + 1);
    }

    Lowl::uint32_l allocate_audio_mixer_id() {
        Lowl::uint32_l mixer_id = next_audio_mixer_id.fetch_add(1, std::memory_order_relaxed);
        if (mixer_id == 0) {
            mixer_id = next_audio_mixer_id.fetch_add(1, std::memory_order_relaxed);
        }
        return mixer_id;
    }
} // namespace

Lowl::Audio::AudioMixer::AudioMixer(const AudioFormat p_audio_format)
    : AudioSource(p_audio_format),
      mixer_id(allocate_audio_mixer_id()) {
    render_connections.fill(RenderConnectionSlot{});
    control_connections.fill(ControlConnectionSlot{});
}

size_t Lowl::Audio::AudioMixer::get_connection_index(const AudioMixerHandle p_handle) {
    if (!p_handle.is_valid() || p_handle.playback_id == 0 || p_handle.playback_id > MAX_CONNECTIONS) {
        return InvalidConnectionIndex;
    }
    return static_cast<size_t>(p_handle.playback_id - 1);
}

size_t Lowl::Audio::AudioMixer::find_free_connection_index_locked() const {
    for (size_t connection_index = 0; connection_index < control_connections.size(); connection_index++) {
        if (!control_connections[connection_index].allocated) {
            return connection_index;
        }
    }
    return InvalidConnectionIndex;
}

Lowl::Audio::AudioMixer::ControlConnectionSlot *
Lowl::Audio::AudioMixer::get_control_connection_locked(const AudioMixerHandle p_handle) {
    if (p_handle.mixer_id != mixer_id) {
        return nullptr;
    }

    const size_t connection_index = get_connection_index(p_handle);
    if (connection_index == InvalidConnectionIndex) {
        return nullptr;
    }

    ControlConnectionSlot &connection = control_connections[connection_index];
    if (!connection.allocated || connection.generation != p_handle.generation) {
        return nullptr;
    }
    return &connection;
}

void Lowl::Audio::AudioMixer::connect_source(const size_t p_connection_index,
                                              const AudioMixerHandle p_handle,
                                              AudioSource *p_audio_source) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        LOWL_LOG_ERROR("AudioMixer::connect_source: connection index is out of range.");
        assert(false && "Reserved mixer connection index must be in range");
        return;
    }
    if (p_audio_source == nullptr) {
        LOWL_LOG_ERROR("AudioMixer::connect_source: queued source is null.");
        assert(false && "A reserved mixer connection must have a source");
        enqueue_completion({AudioMixerCompletion::Type::Removed, p_handle});
        return;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.source != nullptr) {
        LOWL_LOG_ERROR("AudioMixer::connect_source: reserved render slot is already active.");
        assert(false && "A reserved mixer connection must map to an inactive render slot");
        enqueue_completion({AudioMixerCompletion::Type::Removed, p_handle});
        return;
    }
    if (connection.handle == p_handle) {
        LOWL_LOG_ERROR("AudioMixer::connect_source: duplicate connect command.");
        assert(false && "A mixer connection is one-shot");
        return;
    }

    connection.handle = p_handle;
    connection.source = p_audio_source;
    active_source_count++;
    p_audio_source->on_added_to_mixer();
}

void Lowl::Audio::AudioMixer::disconnect_source(const size_t p_connection_index) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        LOWL_LOG_ERROR("AudioMixer::disconnect_source: connection index is out of range.");
        assert(false && "Active mixer connection index must be in range");
        return;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.source == nullptr) {
        return;
    }

    connection.source->on_removed_from_mixer();
    connection.source = nullptr;
    active_source_count--;
}

void Lowl::Audio::AudioMixer::enqueue_completion(const AudioMixerCompletion &p_completion) {
    if (!p_completion.handle.is_valid()) {
        LOWL_LOG_ERROR("AudioMixer::enqueue_completion: completion has an invalid handle.");
        assert(false && "Mixer completion must have a valid handle");
        return;
    }
    if (completions.try_enqueue(p_completion)) {
        return;
    }

    LOWL_LOG_ERROR("AudioMixer::enqueue_completion: completion capacity invariant was violated.");
    assert(false && "One-shot connection slots must bound outstanding completions");
}

void Lowl::Audio::AudioMixer::complete_connection(const size_t p_connection_index,
                                                   const AudioMixerCompletion::Type p_type) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        LOWL_LOG_ERROR("AudioMixer::complete_connection: connection index is out of range.");
        assert(false && "Completed mixer connection index must be in range");
        return;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.source == nullptr || !connection.handle.is_valid()) {
        LOWL_LOG_ERROR("AudioMixer::complete_connection: connection is not active.");
        assert(false && "Only an active mixer connection can complete");
        return;
    }

    const AudioMixerHandle handle = connection.handle;
    disconnect_source(p_connection_index);
    enqueue_completion({p_type, handle});
}

void Lowl::Audio::AudioMixer::process_events() {
    AudioMixerEvent event{};
    while (events.try_dequeue(event)) {
        if (event.handle.mixer_id != mixer_id) {
            LOWL_LOG_ERROR("AudioMixer::process_events: queued handle belongs to a different mixer.");
            assert(false && "Queued mixer handle must belong to this mixer");
            continue;
        }

        const size_t connection_index = get_connection_index(event.handle);
        if (connection_index == InvalidConnectionIndex) {
            LOWL_LOG_ERROR("AudioMixer::process_events: queued connection index is invalid.");
            assert(false && "Queued mixer connection index must be valid");
            continue;
        }

        RenderConnectionSlot &connection = render_connections[connection_index];
        if (event.type == AudioMixerEvent::Type::Connect) {
            connect_source(connection_index, event.handle, event.audio_source);
            continue;
        }

        if (connection.handle != event.handle) {
            LOWL_LOG_ERROR("AudioMixer::process_events: disconnect command is stale.");
            assert(false && "Queued disconnect must match its reserved render connection");
            continue;
        }
        if (connection.source == nullptr) {
            continue;
        }
        complete_connection(connection_index, AudioMixerCompletion::Type::Removed);
    }
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioMixer::render_mixed_block(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    uint32_t produced_frames = 0;
    bool has_output = false;
    bool has_sources = false;
    bool has_error = false;

    size_t remaining_active_sources = active_source_count;
    for (size_t connection_index = 0;
         connection_index < render_connections.size() && remaining_active_sources > 0;
         connection_index++) {
        AudioSource *source = render_connections[connection_index].source;
        if (source == nullptr) {
            continue;
        }
        remaining_active_sources--;
        has_sources = true;

        const RenderResult render_result = source->mix_into(p_block, p_upstream_gain);
        if (render_result.frames_produced > 0) {
            produced_frames = std::max(produced_frames, std::min(render_result.frames_produced, p_block.frame_count));
            has_output = true;
        }

        if (render_result.state == RenderState::Remove) {
            complete_connection(connection_index, AudioMixerCompletion::Type::Finished);
            continue;
        }
        if (render_result.state == RenderState::Error) {
            has_error = true;
        }
    }

    if (!has_output) {
        if (has_error) {
            return {0, RenderState::Error};
        }
        return {0, has_sources ? RenderState::Starved : RenderState::Finished};
    }
    return {produced_frames, has_error ? RenderState::Error : RenderState::Ok};
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioMixer::mix_into(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    process_events();

    if (!playback_enabled.load(std::memory_order_relaxed)) {
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
        for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
            std::fill_n(p_block.channel(channel_index), p_block.frame_count, static_cast<Sample>(0));
        }
        return {0, RenderState::Error};
    }

    return render_mixed_block(p_block, compose_gain_vector(p_upstream_gain));
}

Lowl::AudioMixerHandle Lowl::Audio::AudioMixer::connect(AudioSource *p_audio_source, Error &p_error) {
    if (p_audio_source == nullptr) {
        LOWL_LOG_ERROR("AudioMixer::connect: source must not be null.");
        p_error.set_error(ErrorCode::InvalidParameter);
        return {};
    }

    const AudioFormat &source_format = p_audio_source->get_audio_format();
    const AudioFormat &mixer_format = get_audio_format();
    if (source_format != mixer_format) {
        LOWL_LOG_ERROR("AudioMixer::connect: source format(rate:" + std::to_string(source_format.sample_rate) +
                       ", layout:" + source_format.channel_layout.to_string() + ") does not match mixer(rate:" +
                       std::to_string(mixer_format.sample_rate) + ", layout:" +
                       mixer_format.channel_layout.to_string() + ").");
        p_error.set_error(ErrorCode::InvalidParameter);
        return {};
    }

    std::lock_guard<std::mutex> lock(control_mutex);
    const size_t connection_index = find_free_connection_index_locked();
    if (connection_index == InvalidConnectionIndex) {
        LOWL_LOG_ERROR("AudioMixer::connect: fixed connection capacity is exhausted.");
        p_error.set_error(ErrorCode::InvalidOperationWhileActive);
        return {};
    }

    ControlConnectionSlot &connection = control_connections[connection_index];
    if (connection.generation == 0) {
        connection.generation = 1;
    }

    AudioMixerHandle handle{};
    handle.mixer_id = mixer_id;
    handle.playback_id = static_cast<AudioPlaybackId>(connection_index + 1);
    handle.generation = connection.generation;

    connection.allocated = true;
    connection.disconnect_queued = false;

    AudioMixerEvent event{};
    event.type = AudioMixerEvent::Type::Connect;
    event.handle = handle;
    event.audio_source = p_audio_source;
    if (!events.try_enqueue(event)) {
        connection.allocated = false;
        LOWL_LOG_ERROR("AudioMixer::connect: command queue capacity invariant was violated.");
        p_error.set_error(ErrorCode::Error);
        assert(false && "Reserved connections must bound queued mixer commands");
        return {};
    }

    return handle;
}

void Lowl::Audio::AudioMixer::disconnect(const AudioMixerHandle p_handle, Error &p_error) {
    if (!p_handle.is_valid() || p_handle.mixer_id != mixer_id ||
        get_connection_index(p_handle) == InvalidConnectionIndex) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: handle is invalid or belongs to a different mixer.");
        p_error.set_error(ErrorCode::InvalidParameter);
        return;
    }

    std::lock_guard<std::mutex> lock(control_mutex);
    ControlConnectionSlot *connection = get_control_connection_locked(p_handle);
    if (connection == nullptr) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: handle is stale or already completed.");
        p_error.set_error(ErrorCode::InvalidParameter);
        return;
    }
    if (connection->disconnect_queued) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: connection is already retiring.");
        p_error.set_error(ErrorCode::InvalidOperationWhileActive);
        return;
    }

    AudioMixerEvent event{};
    event.type = AudioMixerEvent::Type::Disconnect;
    event.handle = p_handle;
    if (!events.try_enqueue(event)) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: command queue capacity invariant was violated.");
        p_error.set_error(ErrorCode::Error);
        assert(false && "Reserved connections must bound queued mixer commands");
        return;
    }

    connection->disconnect_queued = true;
}

bool Lowl::Audio::AudioMixer::try_collect_completion(AudioMixerCompletion &p_completion) {
    std::lock_guard<std::mutex> lock(control_mutex);

    AudioMixerCompletion completion{};
    if (!completions.try_dequeue(completion)) {
        return false;
    }

    ControlConnectionSlot *connection = get_control_connection_locked(completion.handle);
    if (connection == nullptr) {
        LOWL_LOG_ERROR("AudioMixer::try_collect_completion: completion is stale or unknown.");
        assert(false && "Mixer completion must match a reserved connection");
        p_completion = completion;
        return true;
    }

    connection->allocated = false;
    connection->disconnect_queued = false;
    connection->generation = advance_generation(connection->generation);
    p_completion = completion;
    return true;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frames_remaining() const {
    return LiveFrameCountSentinel;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_position() const {
    return 0;
}

Lowl::size_l Lowl::Audio::AudioMixer::get_frame_count() const {
    return LiveFrameCountSentinel;
}
