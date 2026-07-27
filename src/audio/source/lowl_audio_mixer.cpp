#include "lowl_audio_mixer.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <string>

#include "lowl_logger.h"

namespace {
    std::atomic<Lowl::AudioInstanceId> next_audio_mixer_id{1};

    bool advance_generation(Lowl::AudioGeneration &p_generation) {
        if (p_generation == std::numeric_limits<Lowl::AudioGeneration>::max()) {
            p_generation = 0;
            return false;
        }
        p_generation++;
        return true;
    }

    Lowl::AudioInstanceId allocate_audio_mixer_id() {
        const Lowl::AudioInstanceId mixer_id = next_audio_mixer_id.fetch_add(1, std::memory_order_relaxed);
        if (mixer_id == 0 || mixer_id == std::numeric_limits<Lowl::AudioInstanceId>::max()) {
            LOWL_LOG_ERROR("AudioMixer: process-wide mixer identity capacity is exhausted.");
            std::abort();
        }
        return mixer_id;
    }

} // namespace

Lowl::Audio::AudioMixer::AudioMixer(const AudioFormat p_audio_format)
    : AudioSource(p_audio_format), mixer_id(allocate_audio_mixer_id()) {
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

void Lowl::Audio::AudioMixer::record_render_fault(const RenderFault p_fault) noexcept {
    render_faults.fetch_or(static_cast<uint32_t>(p_fault), std::memory_order_release);
}

void Lowl::Audio::AudioMixer::report_render_faults_locked() {
    const uint32_t faults = render_faults.load(std::memory_order_acquire);
    const uint32_t new_faults = faults & ~reported_render_faults;
    if (new_faults == 0) {
        return;
    }
    reported_render_faults |= new_faults;

    if ((new_faults & static_cast<uint32_t>(RenderFault::ConnectIndexInvalid)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: queued connection index is invalid; command skipped.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::ConnectSourceMissing)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: queued connection has no source; command skipped.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::ConnectSlotOccupied)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: queued connection targets an occupied slot; command skipped.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::DisconnectIndexInvalid)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: disconnection index is invalid; request skipped.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::CompletionHandleInvalid)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: pending completion handle is invalid; quiescent shutdown is required.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::CompletionQueueFull)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: completion queue invariant was violated; completion retained for retry.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::CompletionIndexInvalid)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: completed connection index is invalid; request skipped.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::CompletionSlotInactive)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: completion targets an inactive slot; request skipped.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::EventIndexInvalid)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: queued event has an invalid connection index; event skipped.");
    }
    if ((new_faults & static_cast<uint32_t>(RenderFault::EventStale)) != 0) {
        LOWL_LOG_ERROR("AudioMixer render: queued event is stale or has no reserved source; event skipped.");
    }
}

bool Lowl::Audio::AudioMixer::connect_source(const size_t p_connection_index,
                                             const AudioMixerHandle p_handle,
                                             AudioSource *p_audio_source) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        record_render_fault(RenderFault::ConnectIndexInvalid);
        return false;
    }
    if (p_audio_source == nullptr) {
        record_render_fault(RenderFault::ConnectSourceMissing);
        return false;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.state != RenderConnectionState::Free || connection.source != nullptr) {
        record_render_fault(RenderFault::ConnectSlotOccupied);
        return false;
    }

    connection.source = p_audio_source;
    connection.generation = p_handle.generation;
    connection.state = RenderConnectionState::Active;
    active_source_count++;
    return true;
}

bool Lowl::Audio::AudioMixer::try_publish_pending_completion(const size_t p_connection_index) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        record_render_fault(RenderFault::CompletionIndexInvalid);
        return false;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.state != RenderConnectionState::CompletionPending || connection.generation == 0) {
        record_render_fault(RenderFault::CompletionSlotInactive);
        return false;
    }

    AudioMixerHandle handle{};
    handle.mixer_id = mixer_id;
    handle.connection_id = static_cast<AudioMixerConnectionId>(p_connection_index + 1);
    handle.generation = connection.generation;
    if (!handle.is_valid()) {
        record_render_fault(RenderFault::CompletionHandleInvalid);
        return false;
    }

    if (pending_completion_count == 0) {
        record_render_fault(RenderFault::CompletionSlotInactive);
        return false;
    }
    if (!completions.try_enqueue(AudioMixerCompletion{connection.pending_completion_type, handle})) {
        record_render_fault(RenderFault::CompletionQueueFull);
        return false;
    }

    connection = {};
    pending_completion_count--;
    return true;
}

bool Lowl::Audio::AudioMixer::flush_pending_completions() {
    if (pending_completion_count == 0) {
        return true;
    }

    for (size_t connection_index = 0; connection_index < render_connections.size() && pending_completion_count > 0;
         connection_index++) {
        if (render_connections[connection_index].state != RenderConnectionState::CompletionPending) {
            continue;
        }
        if (!try_publish_pending_completion(connection_index)) {
            return false;
        }
    }
    return pending_completion_count == 0;
}

bool Lowl::Audio::AudioMixer::complete_connection(const size_t p_connection_index,
                                                  const AudioMixerCompletion::Type p_type) {
    if (p_connection_index == InvalidConnectionIndex || p_connection_index >= render_connections.size()) {
        record_render_fault(RenderFault::CompletionIndexInvalid);
        return false;
    }

    RenderConnectionSlot &connection = render_connections[p_connection_index];
    if (connection.state != RenderConnectionState::Active || connection.source == nullptr ||
        connection.generation == 0) {
        record_render_fault(RenderFault::CompletionSlotInactive);
        return false;
    }

    connection.source = nullptr;
    connection.pending_completion_type = p_type;
    connection.state = RenderConnectionState::CompletionPending;
    active_source_count--;
    pending_completion_count++;
    return try_publish_pending_completion(p_connection_index);
}

bool Lowl::Audio::AudioMixer::process_events() {
    bool events_processed = true;
    AudioMixerEvent event{};
    for (size_t processed_event_count = 0; processed_event_count < MaxEventsPerRender && events.try_dequeue(event);
         processed_event_count++) {
        AudioMixerHandle handle{};
        handle.mixer_id = mixer_id;
        handle.connection_id = event.connection_id;
        handle.generation = event.generation;
        const size_t connection_index = get_connection_index(handle);
        if (connection_index == InvalidConnectionIndex) {
            record_render_fault(RenderFault::EventIndexInvalid);
            events_processed = false;
            continue;
        }

        ControlConnectionSlot &control_connection = control_connections[connection_index];
        if (control_connection.generation != handle.generation || control_connection.source == nullptr) {
            record_render_fault(RenderFault::EventStale);
            events_processed = false;
            continue;
        }
        if (!connect_source(connection_index, handle, control_connection.source)) {
            events_processed = false;
        }
    }
    return events_processed;
}

bool Lowl::Audio::AudioMixer::process_disconnect_requests() {
    bool requests_processed = true;
    size_t remaining_active_sources = active_source_count;
    for (size_t connection_index = 0; connection_index < render_connections.size() && remaining_active_sources > 0;
         connection_index++) {
        if (render_connections[connection_index].source == nullptr) {
            continue;
        }
        remaining_active_sources--;

        if (disconnect_requests[connection_index].load(std::memory_order_acquire)) {
            if (!complete_connection(connection_index, AudioMixerCompletion::Type::Removed)) {
                requests_processed = false;
            }
        }
    }
    return requests_processed;
}

Lowl::Audio::AudioSource::RenderResult
Lowl::Audio::AudioMixer::render_mixed_block(AudioBlockView p_block, const MixGainVector &p_upstream_gain) {
    uint32_t produced_frames = 0;
    bool has_output = false;
    bool has_error = false;

    size_t remaining_active_sources = active_source_count;
    for (size_t connection_index = 0; connection_index < render_connections.size() && remaining_active_sources > 0;
         connection_index++) {
        AudioSource *source = render_connections[connection_index].source;
        if (source == nullptr) {
            continue;
        }
        remaining_active_sources--;

        if (disconnect_requests[connection_index].load(std::memory_order_acquire)) {
            if (!complete_connection(connection_index, AudioMixerCompletion::Type::Removed)) {
                has_error = true;
            }
            continue;
        }

        const RenderResult render_result = source->mix_into(p_block, p_upstream_gain);
        if (render_result.frames_produced > 0) {
            produced_frames = std::max(produced_frames, std::min(render_result.frames_produced, p_block.frame_count));
            has_output = true;
        }

        if (render_result.state == RenderState::Remove) {
            if (!complete_connection(connection_index, AudioMixerCompletion::Type::Finished)) {
                has_error = true;
            }
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

Lowl::Audio::AudioSource::RenderResult Lowl::Audio::AudioMixer::mix_into(AudioBlockView p_block,
                                                                         const MixGainVector &p_upstream_gain) {
    if (shut_down.load(std::memory_order_acquire)) {
        return {0, RenderState::Error};
    }

    const bool pending_completions_flushed = flush_pending_completions();
    const bool events_processed = process_events();

    if (!playback_enabled.load(std::memory_order_relaxed)) {
        const bool requests_processed = process_disconnect_requests();
        return {0,
                pending_completions_flushed && events_processed && requests_processed ? RenderState::Starved
                                                                                      : RenderState::Error};
    }

    const uint8_t expected_channel_count = get_channel_count();
    if (p_block.channel_count != expected_channel_count) {
        process_disconnect_requests();
        return {0, RenderState::Error};
    }

    RenderResult result = render_mixed_block(p_block, compose_gain_vector(p_upstream_gain));
    if (!pending_completions_flushed || !events_processed) {
        result.state = RenderState::Error;
    }
    return result;
}

Lowl::AudioMixerHandle Lowl::Audio::AudioMixer::connect(AudioSource &p_audio_source, Error &p_error) {
    p_error.clear();

    std::lock_guard<std::mutex> lock(control_mutex);
    report_render_faults_locked();
    if (shut_down.load(std::memory_order_relaxed)) {
        LOWL_LOG_ERROR("AudioMixer::connect: mixer has been shut down.");
        p_error.set_error(ErrorCode::MixerShutdown);
        return {};
    }

    const AudioFormat &source_format = p_audio_source.get_audio_format();
    const AudioFormat &mixer_format = get_audio_format();
    if (source_format != mixer_format) {
        LOWL_LOG_ERROR("AudioMixer::connect: source format(rate:" + std::to_string(source_format.sample_rate) +
                       ", layout:" + source_format.channel_layout.to_string() +
                       ") does not match mixer(rate:" + std::to_string(mixer_format.sample_rate) +
                       ", layout:" + mixer_format.channel_layout.to_string() + ").");
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
    handle.connection_id = static_cast<AudioMixerConnectionId>(connection_index + 1);
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
        return {};
    }

    return handle;
}

void Lowl::Audio::AudioMixer::disconnect(const AudioMixerHandle p_handle, Error &p_error) {
    p_error.clear();

    std::lock_guard<std::mutex> lock(control_mutex);
    report_render_faults_locked();
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
    report_render_faults_locked();
    AudioMixerCompletion completion{};
    if (!completions.try_dequeue(completion)) {
        return false;
    }

    ControlConnectionSlot *connection = get_control_connection_locked(completion.handle);
    if (connection == nullptr) {
        LOWL_LOG_ERROR("AudioMixer::try_collect_completion: completion is stale or unknown.");
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
    report_render_faults_locked();
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
    pending_completion_count = 0;

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
