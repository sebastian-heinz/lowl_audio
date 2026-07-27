#include <doctest/doctest.h>
#include <lowl.h>

#include <atomic>
#include <memory>
#include <vector>

#include "audio/lowl_audio_buffer.h"

namespace {
    Lowl::Audio::AudioFormat stereo_format() {
        return {44100.0, Lowl::Audio::ChannelLayout::Stereo};
    }

    class ConstantGraphSource final : public Lowl::Audio::AudioSource {
    public:
        ConstantGraphSource(const Lowl::Sample p_left,
                            const Lowl::Sample p_right,
                            std::shared_ptr<std::atomic<size_t>> p_destruction_count = nullptr)
            : AudioSource(stereo_format()), left(p_left), right(p_right),
              destruction_count(std::move(p_destruction_count)) {
        }

        ~ConstantGraphSource() override {
            if (destruction_count) {
                destruction_count->fetch_add(1, std::memory_order_relaxed);
            }
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block, const MixGainVector &) override {
            if (p_block.channel_count != 2) {
                return {0, RenderState::Error};
            }
            for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
                p_block.channel(0)[frame_index] += left;
                p_block.channel(1)[frame_index] += right;
            }
            return {p_block.frame_count, p_block.frame_count == 0 ? RenderState::Starved : RenderState::Ok};
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

    private:
        Lowl::Sample left = 0;
        Lowl::Sample right = 0;
        std::shared_ptr<std::atomic<size_t>> destruction_count;
    };

    class FiniteGraphSource final : public Lowl::Audio::AudioSource {
    public:
        FiniteGraphSource() : AudioSource(stereo_format()) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block, const MixGainVector &) override {
            if (finished || p_block.frame_count == 0) {
                return {0, finished ? RenderState::Remove : RenderState::Starved};
            }
            p_block.channel(0)[0] += static_cast<Lowl::Sample>(0.25);
            p_block.channel(1)[0] += static_cast<Lowl::Sample>(-0.5);
            finished = true;
            return {1, RenderState::Remove};
        }

        Lowl::size_l get_frames_remaining() const override {
            return finished ? 0 : 1;
        }

        Lowl::size_l get_frame_position() const override {
            return finished ? 1 : 0;
        }

        Lowl::size_l get_frame_count() const override {
            return 1;
        }

    private:
        bool finished = false;
    };

    Lowl::Audio::AudioSource::RenderResult render_one_frame(Lowl::Audio::AudioGraph &p_graph,
                                                            Lowl::Audio::AudioBuffer &p_buffer) {
        p_buffer.clear(1);
        return p_graph.render(p_buffer.view(1));
    }
} // namespace

TEST_CASE("AudioGraph lifecycle") {
    SUBCASE("disconnecting a paused graph preserves and reconnects the complete subtree") {
        Lowl::Audio::AudioGraph graph(stereo_format());
        Lowl::Error error;

        const Lowl::AudioNodeHandle submixer = graph.create<Lowl::Audio::AudioMixer>(error, stereo_format());
        REQUIRE_FALSE(error.has_error());
        REQUIRE(submixer.is_valid());

        const Lowl::AudioNodeHandle source =
            graph.create<ConstantGraphSource>(error, static_cast<Lowl::Sample>(0.25), static_cast<Lowl::Sample>(-0.5));
        REQUIRE_FALSE(error.has_error());
        REQUIRE(source.is_valid());

        graph.connect(graph.root(), submixer, error);
        REQUIRE_FALSE(error.has_error());
        graph.connect(submixer, source, error);
        REQUIRE_FALSE(error.has_error());

        Lowl::Audio::AudioBuffer buffer(1, 2);
        const Lowl::Audio::AudioSource::RenderResult first_result = render_one_frame(graph, buffer);
        REQUIRE_EQ(first_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(buffer.view(1).channel(0)[0], doctest::Approx(0.25));
        REQUIRE_EQ(buffer.view(1).channel(1)[0], doctest::Approx(-0.5));

        graph.pause();
        graph.disconnect(submixer, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(graph.is_retiring(submixer, error));
        REQUIRE_FALSE(error.has_error());

        const Lowl::Audio::AudioSource::RenderResult paused_result = render_one_frame(graph, buffer);
        REQUIRE_EQ(paused_result.state, Lowl::Audio::AudioSource::RenderState::Starved);
        graph.update();

        REQUIRE(graph.contains(submixer));
        REQUIRE(graph.contains(source));
        REQUIRE_FALSE(graph.is_connected(submixer, error));
        REQUIRE_FALSE(error.has_error());
        REQUIRE(graph.is_connected(source, error));
        REQUIRE_FALSE(error.has_error());

        graph.connect(graph.root(), submixer, error);
        REQUIRE_FALSE(error.has_error());
        graph.play();

        const Lowl::Audio::AudioSource::RenderResult reconnected_result = render_one_frame(graph, buffer);
        REQUIRE_EQ(reconnected_result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(buffer.view(1).channel(0)[0], doctest::Approx(0.25));
        REQUIRE_EQ(buffer.view(1).channel(1)[0], doctest::Approx(-0.5));
    }

    SUBCASE("destroying an active subtree while paused waits for acknowledgement") {
        Lowl::Audio::AudioGraph graph(stereo_format());
        Lowl::Error error;
        auto destruction_count = std::make_shared<std::atomic<size_t>>(0);

        const Lowl::AudioNodeHandle submixer = graph.create<Lowl::Audio::AudioMixer>(error, stereo_format());
        const Lowl::AudioNodeHandle source = graph.create<ConstantGraphSource>(
            error, static_cast<Lowl::Sample>(0.25), static_cast<Lowl::Sample>(0.5), destruction_count);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(submixer.is_valid());
        REQUIRE(source.is_valid());

        graph.connect(graph.root(), submixer, error);
        REQUIRE_FALSE(error.has_error());
        graph.connect(submixer, source, error);
        REQUIRE_FALSE(error.has_error());

        Lowl::Audio::AudioBuffer buffer(1, 2);
        REQUIRE_NE(render_one_frame(graph, buffer).state, Lowl::Audio::AudioSource::RenderState::Error);

        graph.pause();
        graph.destroy(submixer, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(graph.contains(submixer));
        REQUIRE(graph.contains(source));
        REQUIRE(graph.is_retiring(submixer, error));
        REQUIRE_FALSE(error.has_error());

        REQUIRE_EQ(render_one_frame(graph, buffer).state, Lowl::Audio::AudioSource::RenderState::Starved);
        graph.update();

        REQUIRE_FALSE(graph.contains(submixer));
        REQUIRE_FALSE(graph.contains(source));
        REQUIRE_EQ(graph.get_node_count(), 1U);
        REQUIRE_EQ(destruction_count->load(std::memory_order_relaxed), 1U);
    }

    SUBCASE("a naturally finished source becomes a detached reusable node") {
        Lowl::Audio::AudioGraph graph(stereo_format());
        Lowl::Error error;
        const Lowl::AudioNodeHandle source = graph.create<FiniteGraphSource>(error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE(source.is_valid());

        graph.connect(graph.root(), source, error);
        REQUIRE_FALSE(error.has_error());

        Lowl::Audio::AudioBuffer buffer(1, 2);
        const Lowl::Audio::AudioSource::RenderResult result = render_one_frame(graph, buffer);
        REQUIRE_EQ(result.frames_produced, 1U);
        REQUIRE_EQ(result.state, Lowl::Audio::AudioSource::RenderState::Ok);
        REQUIRE_EQ(buffer.view(1).channel(0)[0], doctest::Approx(0.25));
        REQUIRE_EQ(buffer.view(1).channel(1)[0], doctest::Approx(-0.5));

        graph.update();
        REQUIRE(graph.contains(source));
        REQUIRE_FALSE(graph.is_connected(source, error));
        REQUIRE_FALSE(error.has_error());

        graph.destroy(source, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE_FALSE(graph.contains(source));
        REQUIRE_EQ(graph.get_node_count(), 1U);
    }

    SUBCASE("handles are graph-scoped and destroyed node identities stay stale") {
        Lowl::Audio::AudioGraph first_graph(stereo_format());
        Lowl::Audio::AudioGraph second_graph(stereo_format());
        Lowl::Error error;

        const Lowl::AudioNodeHandle source = first_graph.create<ConstantGraphSource>(
            error, static_cast<Lowl::Sample>(0.25), static_cast<Lowl::Sample>(0.5));
        REQUIRE_FALSE(error.has_error());
        REQUIRE(source.is_valid());

        second_graph.connect(second_graph.root(), source, error);
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::GraphNodeInvalid);
        REQUIRE(first_graph.contains(source));
        REQUIRE_FALSE(second_graph.contains(source));

        first_graph.destroy(source, error);
        REQUIRE_FALSE(error.has_error());
        REQUIRE_FALSE(first_graph.contains(source));
        REQUIRE_EQ(first_graph.get_source(source, error), nullptr);
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::GraphNodeInvalid);

        const Lowl::AudioNodeHandle replacement = first_graph.create<ConstantGraphSource>(
            error, static_cast<Lowl::Sample>(0.5), static_cast<Lowl::Sample>(0.25));
        REQUIRE_FALSE(error.has_error());
        REQUIRE(replacement.is_valid());
        REQUIRE_NE(replacement.node_id, source.node_id);
    }

    SUBCASE("fixed node and topology-depth boundaries fail synchronously") {
        {
            Lowl::Audio::AudioGraph graph(stereo_format());
            Lowl::Error error;
            std::vector<Lowl::AudioNodeHandle> handles;
            handles.reserve(Lowl::Audio::AudioGraph::MaxNodes - 1);

            for (size_t index = 1; index < Lowl::Audio::AudioGraph::MaxNodes; index++) {
                const Lowl::AudioNodeHandle handle = graph.create<ConstantGraphSource>(
                    error, static_cast<Lowl::Sample>(0), static_cast<Lowl::Sample>(0));
                REQUIRE_FALSE(error.has_error());
                REQUIRE(handle.is_valid());
                handles.push_back(handle);
            }

            const Lowl::AudioNodeHandle exhausted =
                graph.create<ConstantGraphSource>(error, static_cast<Lowl::Sample>(0), static_cast<Lowl::Sample>(0));
            REQUIRE_FALSE(exhausted.is_valid());
            REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::GraphNodeCapacityExhausted);
            REQUIRE_EQ(graph.get_node_count(), Lowl::Audio::AudioGraph::MaxNodes);
        }

        {
            Lowl::Audio::AudioGraph graph(stereo_format());
            Lowl::Error error;
            Lowl::AudioNodeHandle parent = graph.root();

            for (size_t depth = 1; depth < Lowl::Audio::AudioGraph::MaxTopologyDepth; depth++) {
                const Lowl::AudioNodeHandle child = graph.create<Lowl::Audio::AudioMixer>(error, stereo_format());
                REQUIRE_FALSE(error.has_error());
                REQUIRE(child.is_valid());
                graph.connect(parent, child, error);
                REQUIRE_FALSE(error.has_error());
                parent = child;
            }

            const Lowl::AudioNodeHandle too_deep =
                graph.create<ConstantGraphSource>(error, static_cast<Lowl::Sample>(0), static_cast<Lowl::Sample>(0));
            REQUIRE_FALSE(error.has_error());
            graph.connect(parent, too_deep, error);
            REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::GraphTopologyDepthExceeded);
            REQUIRE_FALSE(graph.is_connected(too_deep, error));
            REQUIRE_FALSE(error.has_error());
        }
    }
}
