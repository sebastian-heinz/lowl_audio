#include <doctest/doctest.h>

#include "audio/source/lowl_audio_source.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {
    class NameProbeSource final : public Lowl::Audio::AudioSource {
    public:
        NameProbeSource()
            : AudioSource(44100.0, Lowl::Audio::AudioChannel::Stereo) {
        }

        RenderResult render(Lowl::Audio::AudioBlockView) override {
            return {0, RenderState::Starved};
        }

        Lowl::size_l get_frames_remaining() const override {
            return 0;
        }

        Lowl::size_l get_frame_position() const override {
            return 0;
        }

        Lowl::size_l get_frame_count() const override {
            return 0;
        }
    };
} // namespace

TEST_CASE("AudioSource") {
    SUBCASE("AudioSource - name stays consistent during concurrent reads and writes") {
        NameProbeSource source;
        const std::string short_name = "voice-a";
        const std::string long_name =
            "voice-with-a-much-longer-debug-name-to-force-string-storage-changes-and-copy-paths";

        source.set_name(short_name);

        std::atomic<bool> start{false};
        std::atomic<int> invalid_reads{0};
        std::vector<std::thread> threads;

        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
            }
            for (int index = 0; index < 4000; index++) {
                source.set_name((index % 2) == 0 ? short_name : long_name);
            }
        });

        for (int thread_index = 0; thread_index < 4; thread_index++) {
            threads.emplace_back([&]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (int index = 0; index < 4000; index++) {
                    const std::string name = source.get_name();
                    if (name != short_name && name != long_name) {
                        invalid_reads.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (std::thread &thread : threads) {
            thread.join();
        }

        REQUIRE_EQ(invalid_reads.load(std::memory_order_relaxed), 0);
    }
}
