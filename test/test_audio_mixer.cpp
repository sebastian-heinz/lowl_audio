#include <doctest/doctest.h>
#include <lowl.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "audio/lowl_audio_buffer.h"

namespace {
    Lowl::Audio::AudioFormat stereo_format() {
        return {44100.0, Lowl::Audio::ChannelLayout::Stereo};
    }

    class SilentLiveSource final : public Lowl::Audio::AudioSource {
    public:
        SilentLiveSource() : AudioSource(stereo_format()) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView, const MixGainVector &) override {
            return {0, RenderState::Starved};
        }

        Lowl::size_l get_frames_remaining() const override {
            return 1;
        }

        Lowl::size_l get_frame_position() const override {
            return 0;
        }

        Lowl::size_l get_frame_count() const override {
            return 1;
        }
    };

    class FiniteSource final : public Lowl::Audio::AudioSource {
    public:
        FiniteSource() : AudioSource(stereo_format()) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView, const MixGainVector &) override {
            render_count++;
            return {0, RenderState::Remove};
        }

        Lowl::size_l get_frames_remaining() const override {
            return 0;
        }

        Lowl::size_l get_frame_position() const override {
            return 1;
        }

        Lowl::size_l get_frame_count() const override {
            return 1;
        }

        size_t render_count = 0;
    };

    void process_all_connect_events(Lowl::Audio::AudioMixer &p_mixer, Lowl::Audio::AudioBuffer &p_buffer) {
        const size_t render_pass_count =
            (Lowl::Audio::AudioMixer::MaxConnections + Lowl::Audio::AudioMixer::MaxEventsPerRender - 1) /
            Lowl::Audio::AudioMixer::MaxEventsPerRender;
        for (size_t pass = 0; pass < render_pass_count; pass++) {
            p_buffer.clear(1);
            const Lowl::Audio::AudioSource::RenderResult result = p_mixer.render(p_buffer.view(1));
            REQUIRE_NE(result.state, Lowl::Audio::AudioSource::RenderState::Error);
        }
    }
} // namespace

TEST_CASE("AudioMixer completion lifetime") {
    SUBCASE("all reserved connections can publish Removed completions before collection") {
        Lowl::Audio::AudioMixer mixer(stereo_format());
        Lowl::Error error;
        std::vector<std::unique_ptr<SilentLiveSource>> sources;
        std::vector<Lowl::AudioMixerHandle> handles;
        sources.reserve(Lowl::Audio::AudioMixer::MaxConnections + 1);
        handles.reserve(Lowl::Audio::AudioMixer::MaxConnections);

        for (size_t index = 0; index < Lowl::Audio::AudioMixer::MaxConnections; index++) {
            sources.push_back(std::make_unique<SilentLiveSource>());
            const Lowl::AudioMixerHandle handle = mixer.connect(*sources.back(), error);
            REQUIRE_FALSE(error.has_error());
            REQUIRE(handle.is_valid());
            handles.push_back(handle);
        }

        Lowl::Audio::AudioBuffer buffer(1, 2);
        process_all_connect_events(mixer, buffer);

        for (const Lowl::AudioMixerHandle handle : handles) {
            mixer.disconnect(handle, error);
            REQUIRE_FALSE(error.has_error());
        }

        buffer.clear(1);
        const Lowl::Audio::AudioSource::RenderResult result = mixer.render(buffer.view(1));
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Starved);

        sources.push_back(std::make_unique<SilentLiveSource>());
        const Lowl::AudioMixerHandle exhausted_handle = mixer.connect(*sources.back(), error);
        REQUIRE_FALSE(exhausted_handle.is_valid());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::MixerCapacityExhausted);

        std::vector<bool> completion_seen(Lowl::Audio::AudioMixer::MaxConnections, false);
        for (size_t completion_index = 0; completion_index < Lowl::Audio::AudioMixer::MaxConnections;
             completion_index++) {
            Lowl::Audio::AudioMixerCompletion completion{};
            REQUIRE(mixer.try_collect_completion(completion));
            REQUIRE_EQ(completion.type, Lowl::Audio::AudioMixerCompletion::Type::Removed);
            REQUIRE_GE(completion.handle.connection_id, 1);
            REQUIRE_LE(completion.handle.connection_id, Lowl::Audio::AudioMixer::MaxConnections);

            const size_t handle_index = static_cast<size_t>(completion.handle.connection_id - 1);
            REQUIRE_FALSE(completion_seen[handle_index]);
            REQUIRE(completion.handle == handles[handle_index]);
            completion_seen[handle_index] = true;
        }

        Lowl::Audio::AudioMixerCompletion completion{};
        REQUIRE_FALSE(mixer.try_collect_completion(completion));

        for (size_t index = 0; index < handles.size(); index++) {
            const Lowl::AudioMixerHandle recycled_handle = mixer.connect(*sources[index], error);
            REQUIRE_FALSE(error.has_error());
            REQUIRE_EQ(recycled_handle.connection_id, handles[index].connection_id);
            REQUIRE_NE(recycled_handle.generation, handles[index].generation);
        }
    }

    SUBCASE("all reserved finite sources can publish Finished completions before collection") {
        Lowl::Audio::AudioMixer mixer(stereo_format());
        Lowl::Error error;
        std::vector<std::unique_ptr<FiniteSource>> sources;
        std::vector<Lowl::AudioMixerHandle> handles;
        sources.reserve(Lowl::Audio::AudioMixer::MaxConnections + 1);
        handles.reserve(Lowl::Audio::AudioMixer::MaxConnections);

        for (size_t index = 0; index < Lowl::Audio::AudioMixer::MaxConnections; index++) {
            sources.push_back(std::make_unique<FiniteSource>());
            const Lowl::AudioMixerHandle handle = mixer.connect(*sources.back(), error);
            REQUIRE_FALSE(error.has_error());
            REQUIRE(handle.is_valid());
            handles.push_back(handle);
        }

        Lowl::Audio::AudioBuffer buffer(1, 2);
        process_all_connect_events(mixer, buffer);

        for (const std::unique_ptr<FiniteSource> &source : sources) {
            REQUIRE_EQ(source->render_count, 1U);
        }

        sources.push_back(std::make_unique<FiniteSource>());
        const Lowl::AudioMixerHandle exhausted_handle = mixer.connect(*sources.back(), error);
        REQUIRE_FALSE(exhausted_handle.is_valid());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::MixerCapacityExhausted);

        std::vector<bool> completion_seen(Lowl::Audio::AudioMixer::MaxConnections, false);
        for (size_t completion_index = 0; completion_index < Lowl::Audio::AudioMixer::MaxConnections;
             completion_index++) {
            Lowl::Audio::AudioMixerCompletion completion{};
            REQUIRE(mixer.try_collect_completion(completion));
            REQUIRE_EQ(completion.type, Lowl::Audio::AudioMixerCompletion::Type::Finished);

            const size_t handle_index = static_cast<size_t>(completion.handle.connection_id - 1);
            REQUIRE_LT(handle_index, handles.size());
            REQUIRE_FALSE(completion_seen[handle_index]);
            REQUIRE(completion.handle == handles[handle_index]);
            completion_seen[handle_index] = true;
        }

        Lowl::Audio::AudioMixerCompletion completion{};
        REQUIRE_FALSE(mixer.try_collect_completion(completion));
    }

    SUBCASE("control and render threads repeatedly connect retire collect and reuse slots") {
        constexpr size_t source_count = 64;
        constexpr size_t cycle_count = 32;

        Lowl::Audio::AudioMixer mixer(stereo_format());
        std::vector<std::unique_ptr<SilentLiveSource>> sources;
        sources.reserve(source_count);
        for (size_t index = 0; index < source_count; index++) {
            sources.push_back(std::make_unique<SilentLiveSource>());
        }

        std::atomic<bool> stop_rendering{false};
        std::atomic<bool> render_error{false};
        std::thread render_thread([&]() {
            Lowl::Audio::AudioBuffer buffer(1, 2);
            while (!stop_rendering.load(std::memory_order_acquire)) {
                buffer.clear(1);
                const Lowl::Audio::AudioSource::RenderResult result = mixer.render(buffer.view(1));
                if (result.state == Lowl::Audio::AudioSource::RenderState::Error) {
                    render_error.store(true, std::memory_order_release);
                    return;
                }
                std::this_thread::yield();
            }
        });

        bool lifecycle_error = false;
        bool invalid_completion = false;
        bool timed_out = false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

        for (size_t cycle = 0; cycle < cycle_count && !lifecycle_error && !invalid_completion; cycle++) {
            std::vector<Lowl::AudioMixerHandle> handles;
            handles.reserve(source_count);
            Lowl::Error error;

            for (size_t index = 0; index < source_count; index++) {
                const Lowl::AudioMixerHandle handle = mixer.connect(*sources[index], error);
                if (error.has_error() || !handle.is_valid()) {
                    lifecycle_error = true;
                    break;
                }
                handles.push_back(handle);
            }
            for (const Lowl::AudioMixerHandle handle : handles) {
                mixer.disconnect(handle, error);
                if (error.has_error()) {
                    lifecycle_error = true;
                    break;
                }
            }

            size_t collected_count = 0;
            std::vector<bool> completion_seen(source_count, false);
            while (!lifecycle_error && !invalid_completion && collected_count < handles.size()) {
                if (std::chrono::steady_clock::now() >= deadline) {
                    timed_out = true;
                    break;
                }

                Lowl::Audio::AudioMixerCompletion completion{};
                if (!mixer.try_collect_completion(completion)) {
                    std::this_thread::yield();
                    continue;
                }
                if (completion.type != Lowl::Audio::AudioMixerCompletion::Type::Removed ||
                    completion.handle.connection_id == 0 || completion.handle.connection_id > handles.size()) {
                    invalid_completion = true;
                    break;
                }

                const size_t handle_index = static_cast<size_t>(completion.handle.connection_id - 1);
                if (completion_seen[handle_index] || completion.handle != handles[handle_index]) {
                    invalid_completion = true;
                    break;
                }
                completion_seen[handle_index] = true;
                collected_count++;
            }
            if (timed_out) {
                break;
            }
        }

        stop_rendering.store(true, std::memory_order_release);
        render_thread.join();

        REQUIRE_FALSE(timed_out);
        REQUIRE_FALSE(lifecycle_error);
        REQUIRE_FALSE(invalid_completion);
        REQUIRE_FALSE(render_error.load(std::memory_order_acquire));
    }
}
