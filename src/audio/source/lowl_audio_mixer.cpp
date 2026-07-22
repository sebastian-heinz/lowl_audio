#include "lowl_audio_mixer.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <string>

#include "lowl_logger.h"

namespace {
    std::atomic<Lowl::uint64_l> next_audio_mixer_id{1};

    bool advance_generation(Lowl::uint64_l &p_generation) {
        if (p_generation == std::numeric_limits<Lowl::uint64_l>::max()) {
            p_generation = 0;
            return false;
        }
        p_generation++;
        return true;
    }

    Lowl::uint64_l allocate_audio_mixer_id() {
        const Lowl::uint64_l mixer_id = next_audio_mixer_id.fetch_add(1, std::memory_order_relaxed);
        if (mixer_id == 0 || mixer_id == std::numeric_limits<Lowl::uint64_l>::max()) {
            LOWL_LOG_ERROR("AudioMixer: process-wide mixer identity capacity is exhausted.");
            std::abort();
        }
        return mixer_id;
    }

} // namespace

Lowl::Audio::AudioMixer::AudioMixer(const AudioFormat p_audio_format)
    : AudioSource(p_audio_format),
      mixer_id(allocate_audio_mixer_id()) {
}

Lowl::Audio::AudioMixer::~AudioMixer() {
    shutdown_quiescent();
}

size_t Lowl::Audio::AudioMixer::get_connection_index(const AudioMixerHandle p_handle) {
    if (!p_handle.is_valid() || p_handle.connection_id == 0 || p_handle.connection_id > MaxConnections) {
        return InvalidConnectionIndex;
    }
    return static_cast<size_t>(p_handle.connection_id - 1);
}

size_t Lowl::Audio::AudioMixer::find_free_connection_index_locked() const {
    for (size_t connection_index = 0; connection_index < control_connections.size(); connection_index++) {
        const ControlConnectionSlot &connection = control_connections[connection_index];
        if (connection.source == nullptr && connection.generation != 0) {
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
    if (connection.source == nullptr || connection.generation != p_handle.generation) {
        return nullptr;
    }
    return &connection;
}

void Lowl::Audio::AudioMixer::connect_source(const size_t p_connection_index,
                                              const AudioMixerHandle p_handle,
                                              AudioSource *p_audio_source) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        assert(false && "Reserved mixer connection index must be in range");
        return;
    }
    if (p_audio_source == nullptr) {
        assert(false && "A reserved mixer connection must have a source");
        enqueue_completion({AudioMixerCompletion::Type::Removed, p_handle});
        return;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.source != nullptr) {
        assert(false && "A reserved mixer connection must map to an inactive render slot");
        enqueue_completion({AudioMixerCompletion::Type::Removed, p_handle});
        return;
    }

    connection.source = p_audio_source;
    connection.generation = p_handle.generation;
    active_source_count++;
}

void Lowl::Audio::AudioMixer::disconnect_source(const size_t p_connection_index) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        assert(false && "Active mixer connection index must be in range");
        return;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.source == nullptr) {
        return;
    }

    connection.source = nullptr;
    active_source_count--;
}

void Lowl::Audio::AudioMixer::enqueue_completion(const AudioMixerCompletion &p_completion) {
    if (!p_completion.handle.is_valid()) {
        assert(false && "Mixer completion must have a valid handle");
        return;
    }
    if (completions.try_enqueue(p_completion)) {
        return;
    }

    assert(false && "One-shot connection slots must bound outstanding completions");
}

void Lowl::Audio::AudioMixer::complete_connection(const size_t p_connection_index,
                                                   const AudioMixerCompletion::Type p_type) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        assert(false && "Completed mixer connection index must be in range");
        return;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.source == nullptr || connection.generation == 0) {
        assert(false && "Only an active mixer connection can complete");
        return;
    }

    AudioMixerHandle handle{};
    handle.mixer_id = mixer_id;
    handle.connection_id = static_cast<AudioPlaybackId>(p_connection_index + 1);
    handle.generation = connection.generation;
    disconnect_source(p_connection_index);
    enqueue_completion({p_type, handle});
}

void Lowl::Audio::AudioMixer::process_events() {
    AudioMixerEvent event{};
    for (size_t processed_event_count = 0;
         processed_event_count < MaxEventsPerRender && events.try_dequeue(event);
         processed_event_count++) {
        AudioMixerHandle handle{};
        handle.mixer_id = mixer_id;
        handle.connection_id = event.connection_id;
        handle.generation = event.generation;
        const size_t connection_index = get_connection_index(handle);
        if (connection_index == InvalidConnectionIndex) {
            assert(false && "Queued mixer connection index must be valid");
            continue;
        }

        ControlConnectionSlot &control_connection = control_connections[connection_index];
        if (control_connection.generation != handle.generation || control_connection.source == nullptr) {
            assert(false && "Queued connect must match its reserved control connection");
            continue;
        }
        connect_source(connection_index, handle, control_connection.source);
    }
}

void Lowl::Audio::AudioMixer::process_disconnect_requests() {
    size_t remaining_active_sources = active_source_count;
    for (size_t connection_index = 0;
         connection_index < render_connections.size() && remaining_active_sources > 0;
         connection_index++) {
        if (render_connections[connection_index].source == nullptr) {
            continue;
        }
        remaining_active_sources--;

        if (disconnect_requests[connection_index].load(std::memory_order_acquire)) {
            complete_connection(connection_index, AudioMixerCompletion::Type::Removed);
        }
    }
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioMixer::render_mixed_block(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    uint32_t produced_frames = 0;
    bool has_output = false;
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

        if (disconnect_requests[connection_index].load(std::memory_order_acquire)) {
            complete_connection(connection_index, AudioMixerCompletion::Type::Removed);
            continue;
        }

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
        return {0, RenderState::Starved};
    }
    return {produced_frames, has_error ? RenderState::Error : RenderState::Ok};
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioMixer::mix_into(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    if (shut_down.load(std::memory_order_acquire)) {
        return {0, RenderState::Error};
    }

    process_events();

    if (!playback_enabled.load(std::memory_order_relaxed)) {
        process_disconnect_requests();
        return {0, RenderState::Starved};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
        process_disconnect_requests();
        return {0, RenderState::Error};
    }

    return render_mixed_block(p_block, compose_gain_vector(p_upstream_gain));
}

Lowl::AudioMixerHandle Lowl::Audio::AudioMixer::connect(AudioSource &p_audio_source, Error &p_error) {
    p_error.clear();

    std::lock_guard<std::mutex> lock(control_mutex);
    if (shut_down.load(std::memory_order_relaxed)) {
        LOWL_LOG_ERROR("AudioMixer::connect: mixer has been shut down.");
        p_error.set_error(ErrorCode::MixerShutdown);
        return {};
    }

    const AudioFormat &source_format = p_audio_source.get_audio_format();
    const AudioFormat &mixer_format = get_audio_format();
    if (source_format != mixer_format) {
        LOWL_LOG_ERROR("AudioMixer::connect: source format(rate:" + std::to_string(source_format.sample_rate) +
                       ", layout:" + source_format.channel_layout.to_string() + ") does not match mixer(rate:" +
                       std::to_string(mixer_format.sample_rate) + ", layout:" +
                       mixer_format.channel_layout.to_string() + ").");
        p_error.set_error(ErrorCode::UnsupportedAudioFormat);
        return {};
    }

    const size_t connection_index = find_free_connection_index_locked();
    if (connection_index == InvalidConnectionIndex) {
        LOWL_LOG_ERROR("AudioMixer::connect: fixed connection capacity is exhausted.");
        p_error.set_error(ErrorCode::MixerCapacityExhausted);
        return {};
    }

    ControlConnectionSlot &connection = control_connections[connection_index];
    AudioMixerHandle handle{};
    handle.mixer_id = mixer_id;
    handle.connection_id = static_cast<AudioPlaybackId>(connection_index + 1);
    handle.generation = connection.generation;

    connection.source = &p_audio_source;
    disconnect_requests[connection_index].store(false, std::memory_order_release);

    AudioMixerEvent event{};
    event.generation = handle.generation;
    event.connection_id = handle.connection_id;
    if (!events.try_enqueue(event)) {
        connection.source = nullptr;
        LOWL_LOG_ERROR("AudioMixer::connect: command queue capacity invariant was violated.");
        p_error.set_error(ErrorCode::Error);
        assert(false && "Reserved connections must bound queued mixer commands");
        return {};
    }

    return handle;
}

void Lowl::Audio::AudioMixer::disconnect(const AudioMixerHandle p_handle, Error &p_error) {
    p_error.clear();

    std::lock_guard<std::mutex> lock(control_mutex);
    if (!p_handle.is_valid() || p_handle.mixer_id != mixer_id ||
        get_connection_index(p_handle) == InvalidConnectionIndex) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: handle is invalid or belongs to a different mixer.");
        p_error.set_error(ErrorCode::MixerConnectionInvalid);
        return;
    }

    if (shut_down.load(std::memory_order_relaxed)) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: mixer has been shut down.");
        p_error.set_error(ErrorCode::MixerShutdown);
        return;
    }
    ControlConnectionSlot *connection = get_control_connection_locked(p_handle);
    if (connection == nullptr) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: handle is stale or already completed.");
        p_error.set_error(ErrorCode::MixerConnectionInvalid);
        return;
    }
    const size_t connection_index = get_connection_index(p_handle);
    if (disconnect_requests[connection_index].exchange(true, std::memory_order_acq_rel)) {
        LOWL_LOG_ERROR("AudioMixer::disconnect: connection is already retiring.");
        p_error.set_error(ErrorCode::InvalidOperationWhileActive);
        return;
    }
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

    connection->source = nullptr;
    const size_t connection_index = get_connection_index(completion.handle);
    disconnect_requests[connection_index].store(false, std::memory_order_release);
    if (!advance_generation(connection->generation)) {
        LOWL_LOG_ERROR("AudioMixer::try_collect_completion: connection generation exhausted; slot retired.");
    }
    p_completion = completion;
    return true;
}

void Lowl::Audio::AudioMixer::shutdown_quiescent() {
    std::lock_guard<std::mutex> lock(control_mutex);
    if (shut_down.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    AudioMixerEvent discarded_event{};
    for (size_t index = 0; index < EventQueueCapacity && events.try_dequeue(discarded_event); index++) {
    }

    AudioMixerCompletion discarded_completion{};
    for (size_t index = 0; index < CompletionQueueCapacity && completions.try_dequeue(discarded_completion); index++) {
    }

    for (RenderConnectionSlot &connection : render_connections) {
        connection = {};
    }
    active_source_count = 0;

    for (ControlConnectionSlot &connection : control_connections) {
        connection.source = nullptr;
    }
    for (std::atomic<bool> &disconnect_request : disconnect_requests) {
        disconnect_request.store(false, std::memory_order_relaxed);
    }
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
