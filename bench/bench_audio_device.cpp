#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "audio/backend/lowl_audio_device.h"
#include "audio/source/lowl_audio_data.h"
#include "audio/source/lowl_audio_mixer.h"
#include "audio/source/lowl_audio_space.h"
#include "audio/source/lowl_audio_stream.h"
#include "lowl_error.h"

namespace {
    constexpr double kSampleRate = 48000.0;
    constexpr uint8_t kStereoChannelCount = 2;
    constexpr uint8_t kMonoChannelCount = 1;
    constexpr size_t kCallbacksPerIteration = 16;
    constexpr double kTwoPi = 6.28318530717958647692;

    Lowl::Audio::AudioDeviceProperties make_device_properties(const Lowl::Audio::SampleFormat p_format) {
        Lowl::Audio::AudioDeviceProperties properties{};
        properties.is_supported = true;
        properties.audio_format = {kSampleRate, Lowl::Audio::ChannelLayout::Stereo};
        properties.sample_format = p_format;
        return properties;
    }

    Lowl::Audio::AudioDeviceProperties make_device_properties(const Lowl::Audio::SampleFormat p_format,
                                                               const Lowl::Audio::ChannelLayout p_layout) {
        Lowl::Audio::AudioDeviceProperties properties{};
        properties.is_supported = true;
        properties.audio_format = {kSampleRate, p_layout};
        properties.sample_format = p_format;
        return properties;
    }

    std::unique_ptr<Lowl::Audio::AudioData> make_stereo_audio_data(const size_t p_frame_count) {
        std::unique_ptr<Lowl::Sample[]> storage;
        if (p_frame_count > 0) {
            storage = std::make_unique<Lowl::Sample[]>(p_frame_count * kStereoChannelCount);
            for (size_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
                const double phase = kTwoPi * static_cast<double>(frame_index) / 96.0;
                storage[frame_index] = static_cast<Lowl::Sample>(0.35 * std::sin(phase));
                storage[p_frame_count + frame_index] = static_cast<Lowl::Sample>(0.35 * std::cos(phase * 0.5));
            }
        }
        return std::make_unique<Lowl::Audio::AudioData>(
            std::move(storage),
            p_frame_count,
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Stereo}
        );
    }

    std::unique_ptr<Lowl::Audio::AudioData> make_mono_audio_data(const size_t p_frame_count) {
        std::unique_ptr<Lowl::Sample[]> storage;
        if (p_frame_count > 0) {
            storage = std::make_unique<Lowl::Sample[]>(p_frame_count);
            for (size_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
                const double phase = kTwoPi * static_cast<double>(frame_index) / 96.0;
                storage[frame_index] = static_cast<Lowl::Sample>(0.35 * std::sin(phase));
            }
        }
        return std::make_unique<Lowl::Audio::AudioData>(
            std::move(storage),
            p_frame_count,
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Mono}
        );
    }

    std::vector<Lowl::Sample> make_interleaved_stereo_frames(const size_t p_frame_count) {
        std::vector<Lowl::Sample> frames(p_frame_count * kStereoChannelCount);
        for (size_t frame_index = 0; frame_index < p_frame_count; frame_index++) {
            const double phase = kTwoPi * static_cast<double>(frame_index) / 64.0;
            frames[frame_index * kStereoChannelCount] = static_cast<Lowl::Sample>(0.30 * std::sin(phase));
            frames[frame_index * kStereoChannelCount + 1] = static_cast<Lowl::Sample>(0.30 * std::cos(phase * 0.75));
        }
        return frames;
    }

    class ConstantStereoSource final : public Lowl::Audio::AudioSource {
    public:
        ConstantStereoSource()
            : AudioSource(Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Stereo}) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block,
                              const MixGainVector &) override {
            for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
                p_block.channel(0)[frame_index] += 0.25f;
                p_block.channel(1)[frame_index] += -0.25f;
            }
            rendered_frames += p_block.frame_count;
            return {p_block.frame_count, RenderState::Ok};
        }

        Lowl::size_l get_frames_remaining() const override {
            return std::numeric_limits<Lowl::size_l>::max();
        }

        Lowl::size_l get_frame_position() const override {
            return rendered_frames;
        }

        Lowl::size_l get_frame_count() const override {
            return std::numeric_limits<Lowl::size_l>::max();
        }

    private:
        Lowl::size_l rendered_frames = 0;
    };

    class ConstantMonoSource final : public Lowl::Audio::AudioSource {
    public:
        ConstantMonoSource()
            : AudioSource(Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Mono}) {
        }

        RenderResult mix_into(Lowl::Audio::AudioBlockView p_block,
                              const MixGainVector &) override {
            for (uint32_t frame_index = 0; frame_index < p_block.frame_count; frame_index++) {
                p_block.channel(0)[frame_index] += 0.25f;
            }
            rendered_frames += p_block.frame_count;
            return {p_block.frame_count, RenderState::Ok};
        }

        Lowl::size_l get_frames_remaining() const override {
            return std::numeric_limits<Lowl::size_l>::max();
        }

        Lowl::size_l get_frame_position() const override {
            return rendered_frames;
        }

        Lowl::size_l get_frame_count() const override {
            return std::numeric_limits<Lowl::size_l>::max();
        }

    private:
        Lowl::size_l rendered_frames = 0;
    };

    class BenchAudioDevice final : public Lowl::Audio::AudioDevice {
    public:
        BenchAudioDevice() : AudioDevice(_constructor_tag()) {
        }

        void configure(const Lowl::Audio::AudioDeviceProperties &p_properties,
                       std::shared_ptr<Lowl::Audio::AudioSource> p_audio_source,
                       unsigned long p_frame_capacity) {
            audio_device_properties = p_properties;
            audio_source = std::move(p_audio_source);
            allocate_render_buffer(p_frame_capacity);
        }

        void write(void *p_dst, unsigned long p_frames_per_buffer, unsigned long p_bytes_per_frame) {
            render_to_device_buffer(load_render_state(),
                                    p_dst,
                                    static_cast<size_t>(p_frames_per_buffer) * p_bytes_per_frame,
                                    p_frames_per_buffer,
                                    p_bytes_per_frame);
        }

        void start(Lowl::Audio::AudioDeviceProperties, std::shared_ptr<Lowl::Audio::AudioSource>, Lowl::Error &) override {
        }

        void stop(Lowl::Error &) override {
        }
    };

    void apply_frame_args(benchmark::Benchmark *p_benchmark) {
        for (const int frames : {64, 128, 256, 512}) {
            p_benchmark->Arg(frames);
        }
    }

    void apply_voice_and_frame_args(benchmark::Benchmark *p_benchmark) {
        for (const int voices : {1, 8, 32, 64, 128, 256}) {
            for (const int frames : {64, 128, 256, 512}) {
                p_benchmark->Args({voices, frames});
            }
        }
    }

    template <typename SampleType>
    void bench_audio_device_render(benchmark::State &state, const Lowl::Audio::SampleFormat p_format) {
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(0));
        constexpr unsigned long channels = kStereoChannelCount;
        const unsigned long bytes_per_frame = sizeof(SampleType) * channels;

        auto source = std::make_shared<ConstantStereoSource>();
        BenchAudioDevice device;
        device.configure(make_device_properties(p_format), source, frames_per_buffer);

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        for (auto _ : state) {
            benchmark::DoNotOptimize(buffer.data());
            device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer));
        state.SetBytesProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(bytes_per_frame));
    }

    void bench_audio_device_render_format(benchmark::State &state,
                                           const Lowl::Audio::SampleFormat p_format,
                                           const size_t p_sample_bytes) {
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(0));
        constexpr unsigned long channels = kStereoChannelCount;
        const unsigned long bytes_per_frame = static_cast<unsigned long>(p_sample_bytes) * channels;

        auto source = std::make_shared<ConstantStereoSource>();
        BenchAudioDevice device;
        device.configure(make_device_properties(p_format), source, frames_per_buffer);

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        for (auto _ : state) {
            benchmark::DoNotOptimize(buffer.data());
            device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer));
        state.SetBytesProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(bytes_per_frame));
    }

    void bench_audio_device_render_audio_space(benchmark::State &state) {
        const size_t voice_count = static_cast<size_t>(state.range(0));
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(1));
        const unsigned long bytes_per_frame = sizeof(float) * kStereoChannelCount;
        const size_t clip_frames = std::max<size_t>(frames_per_buffer * kCallbacksPerIteration * 8, 32768);

        auto audio_space = std::make_shared<Lowl::Audio::AudioSpace>(
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Stereo});
        audio_space->set_volume(0.9f);
        audio_space->set_panning(0.1f);

        Lowl::Error error;
        const Lowl::AudioAssetHandle asset_handle = audio_space->add_audio(make_stereo_audio_data(clip_frames), error);
        if (error.has_error() || !asset_handle.is_valid()) {
            state.SkipWithError("failed to create AudioSpace asset");
            return;
        }

        std::vector<Lowl::AudioPlaybackHandle> playback_handles;
        playback_handles.reserve(voice_count);
        for (size_t voice_index = 0; voice_index < voice_count; voice_index++) {
            const Lowl::AudioPlaybackHandle playback_handle = audio_space->create_playback(asset_handle);
            if (!playback_handle.is_valid()) {
                state.SkipWithError("failed to create AudioSpace playback");
                return;
            }

            const float base_volume = static_cast<float>(0.8 / static_cast<double>(voice_count));
            const float volume_variation = 0.85f + 0.15f * static_cast<float>(voice_index % 5) / 4.0f;
            const float panning = voice_count == 1
                                      ? 0.0f
                                      : -1.0f + 2.0f * static_cast<float>(voice_index) / static_cast<float>(voice_count - 1);
            audio_space->set_volume(playback_handle, base_volume * volume_variation);
            audio_space->set_panning(playback_handle, panning);
            audio_space->play(playback_handle);
            playback_handles.push_back(playback_handle);
        }

        BenchAudioDevice device;
        device.configure(make_device_properties(Lowl::Audio::SampleFormat::FLOAT_32), audio_space, frames_per_buffer);

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        size_t frames_until_reset = clip_frames;
        for (auto _ : state) {
            for (size_t callback_index = 0; callback_index < kCallbacksPerIteration; callback_index++) {
                benchmark::DoNotOptimize(buffer.data());
                device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
                benchmark::ClobberMemory();
            }

            const size_t frames_consumed = static_cast<size_t>(frames_per_buffer) * kCallbacksPerIteration;
            if (frames_until_reset <= frames_consumed) {
                state.PauseTiming();
                for (const Lowl::AudioPlaybackHandle playback_handle : playback_handles) {
                    audio_space->reset(playback_handle);
                    audio_space->play(playback_handle);
                }
                frames_until_reset = clip_frames;
                state.ResumeTiming();
            } else {
                frames_until_reset -= frames_consumed;
            }
        }

        state.SetItemsProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(kCallbacksPerIteration));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer) *
                                static_cast<int64_t>(kCallbacksPerIteration) * static_cast<int64_t>(bytes_per_frame));
    }

    void bench_audio_device_render_audio_stream(benchmark::State &state) {
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(0));
        const unsigned long bytes_per_frame = sizeof(float) * kStereoChannelCount;
        const size_t stream_capacity = std::max<size_t>(frames_per_buffer * 8, 1024);

        auto stream = std::make_shared<Lowl::Audio::AudioStream>(
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Stereo}, stream_capacity);
        stream->set_volume(0.95f);
        stream->set_panning(-0.2f);
        stream->play();

        const std::vector<Lowl::Sample> write_frames = make_interleaved_stereo_frames(frames_per_buffer);
        const size_t initial_prefill_frames = frames_per_buffer * 4;
        for (size_t remaining = initial_prefill_frames; remaining > 0;) {
            const size_t chunk_frames = std::min<size_t>(remaining, frames_per_buffer);
            const size_t written = stream->write_interleaved(write_frames.data(), chunk_frames);
            if (written == 0) {
                state.SkipWithError("failed to prefill AudioStream");
                return;
            }
            remaining -= written;
        }

        BenchAudioDevice device;
        device.configure(make_device_properties(Lowl::Audio::SampleFormat::FLOAT_32), stream, frames_per_buffer);

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        for (auto _ : state) {
            for (size_t callback_index = 0; callback_index < kCallbacksPerIteration; callback_index++) {
                const size_t written = stream->write_interleaved(write_frames.data(), frames_per_buffer);
                if (written != frames_per_buffer) {
                    state.SkipWithError("AudioStream write did not keep up with render");
                    return;
                }
                benchmark::DoNotOptimize(buffer.data());
                device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
                benchmark::ClobberMemory();
            }
        }

        state.SetItemsProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(kCallbacksPerIteration));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer) *
                                static_cast<int64_t>(kCallbacksPerIteration) * static_cast<int64_t>(bytes_per_frame));
    }

    void bench_audio_mixer_render(benchmark::State &state) {
        const size_t voice_count = static_cast<size_t>(state.range(0));
        const auto frames_per_buffer = static_cast<uint32_t>(state.range(1));

        auto mixer = std::make_shared<Lowl::Audio::AudioMixer>(
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Stereo});
        mixer->set_volume(0.9f);
        mixer->set_panning(0.1f);

        std::vector<std::shared_ptr<ConstantStereoSource>> sources;
        sources.reserve(voice_count);
        for (size_t voice_index = 0; voice_index < voice_count; voice_index++) {
            auto source = std::make_shared<ConstantStereoSource>();
            const Lowl::AudioMixerHandle handle = mixer->allocate_handle();
            if (!handle.is_valid()) {
                state.SkipWithError("failed to allocate mixer handle");
                return;
            }
            mixer->mix(handle, source.get());
            sources.push_back(std::move(source));
        }

        Lowl::Audio::AudioBuffer warmup_buf(frames_per_buffer, kStereoChannelCount);
        warmup_buf.clear(frames_per_buffer);
        mixer->render(warmup_buf.view(frames_per_buffer));

        Lowl::Audio::AudioBuffer render_buffer(frames_per_buffer, kStereoChannelCount);
        for (auto _ : state) {
            render_buffer.clear(frames_per_buffer);
            Lowl::Audio::AudioBlockView block = render_buffer.view(frames_per_buffer);
            benchmark::DoNotOptimize(block.channels.data());
            mixer->render(block);
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer) *
                                static_cast<int64_t>(sizeof(float) * kStereoChannelCount));
    }

    void bench_audio_device_render_audio_space_mono(benchmark::State &state) {
        const size_t voice_count = static_cast<size_t>(state.range(0));
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(1));
        const unsigned long bytes_per_frame = sizeof(float) * kMonoChannelCount;
        const size_t clip_frames = std::max<size_t>(frames_per_buffer * kCallbacksPerIteration * 8, 32768);

        auto audio_space = std::make_shared<Lowl::Audio::AudioSpace>(
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Mono});
        audio_space->set_volume(0.9f);

        Lowl::Error error;
        const Lowl::AudioAssetHandle asset_handle = audio_space->add_audio(make_mono_audio_data(clip_frames), error);
        if (error.has_error() || !asset_handle.is_valid()) {
            state.SkipWithError("failed to create mono AudioSpace asset");
            return;
        }

        std::vector<Lowl::AudioPlaybackHandle> playback_handles;
        playback_handles.reserve(voice_count);
        for (size_t voice_index = 0; voice_index < voice_count; voice_index++) {
            const Lowl::AudioPlaybackHandle playback_handle = audio_space->create_playback(asset_handle);
            if (!playback_handle.is_valid()) {
                state.SkipWithError("failed to create mono AudioSpace playback");
                return;
            }

            const float base_volume = static_cast<float>(0.8 / static_cast<double>(voice_count));
            const float volume_variation = 0.85f + 0.15f * static_cast<float>(voice_index % 5) / 4.0f;
            audio_space->set_volume(playback_handle, base_volume * volume_variation);
            audio_space->play(playback_handle);
            playback_handles.push_back(playback_handle);
        }

        BenchAudioDevice device;
        device.configure(make_device_properties(Lowl::Audio::SampleFormat::FLOAT_32, Lowl::Audio::ChannelLayout::Mono),
                         audio_space, frames_per_buffer);

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        size_t frames_until_reset = clip_frames;
        for (auto _ : state) {
            for (size_t callback_index = 0; callback_index < kCallbacksPerIteration; callback_index++) {
                benchmark::DoNotOptimize(buffer.data());
                device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
                benchmark::ClobberMemory();
            }

            const size_t frames_consumed = static_cast<size_t>(frames_per_buffer) * kCallbacksPerIteration;
            if (frames_until_reset <= frames_consumed) {
                state.PauseTiming();
                for (const Lowl::AudioPlaybackHandle playback_handle : playback_handles) {
                    audio_space->reset(playback_handle);
                    audio_space->play(playback_handle);
                }
                frames_until_reset = clip_frames;
                state.ResumeTiming();
            } else {
                frames_until_reset -= frames_consumed;
            }
        }

        state.SetItemsProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(kCallbacksPerIteration));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer) *
                                static_cast<int64_t>(kCallbacksPerIteration) * static_cast<int64_t>(bytes_per_frame));
    }

    void bench_audio_device_render_audio_stream_planar(benchmark::State &state) {
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(0));
        const unsigned long bytes_per_frame = sizeof(float) * kStereoChannelCount;
        const size_t stream_capacity = std::max<size_t>(frames_per_buffer * 8, 1024);

        auto stream = std::make_shared<Lowl::Audio::AudioStream>(
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Stereo}, stream_capacity);
        stream->set_volume(0.95f);
        stream->set_panning(-0.2f);
        stream->play();

        std::vector<Lowl::Sample> left_channel(frames_per_buffer);
        std::vector<Lowl::Sample> right_channel(frames_per_buffer);
        for (size_t frame_index = 0; frame_index < frames_per_buffer; frame_index++) {
            const double phase = kTwoPi * static_cast<double>(frame_index) / 64.0;
            left_channel[frame_index] = static_cast<Lowl::Sample>(0.30 * std::sin(phase));
            right_channel[frame_index] = static_cast<Lowl::Sample>(0.30 * std::cos(phase * 0.75));
        }
        const std::vector<const Lowl::Sample *> planar_channels = {left_channel.data(), right_channel.data()};

        const size_t initial_prefill_frames = frames_per_buffer * 4;
        for (size_t remaining = initial_prefill_frames; remaining > 0;) {
            const size_t chunk_frames = std::min<size_t>(remaining, frames_per_buffer);
            const size_t written = stream->write_planar(planar_channels, chunk_frames);
            if (written == 0) {
                state.SkipWithError("failed to prefill AudioStream (planar)");
                return;
            }
            remaining -= written;
        }

        BenchAudioDevice device;
        device.configure(make_device_properties(Lowl::Audio::SampleFormat::FLOAT_32), stream, frames_per_buffer);

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        for (auto _ : state) {
            for (size_t callback_index = 0; callback_index < kCallbacksPerIteration; callback_index++) {
                const size_t written = stream->write_planar(planar_channels, frames_per_buffer);
                if (written != frames_per_buffer) {
                    state.SkipWithError("AudioStream planar write did not keep up with render");
                    return;
                }
                benchmark::DoNotOptimize(buffer.data());
                device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
                benchmark::ClobberMemory();
            }
        }

        state.SetItemsProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(kCallbacksPerIteration));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer) *
                                static_cast<int64_t>(kCallbacksPerIteration) * static_cast<int64_t>(bytes_per_frame));
    }

    void bench_audio_device_render_audio_stream_threaded(benchmark::State &state) {
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(0));
        const unsigned long bytes_per_frame = sizeof(float) * kStereoChannelCount;
        const size_t stream_capacity = std::max<size_t>(frames_per_buffer * 8, 1024);

        auto stream = std::make_shared<Lowl::Audio::AudioStream>(
            Lowl::Audio::AudioFormat{kSampleRate, Lowl::Audio::ChannelLayout::Stereo}, stream_capacity);
        stream->set_volume(0.95f);
        stream->set_panning(-0.2f);
        stream->play();

        const std::vector<Lowl::Sample> write_frames = make_interleaved_stereo_frames(frames_per_buffer);

        const size_t initial_prefill_frames = frames_per_buffer * 4;
        for (size_t remaining = initial_prefill_frames; remaining > 0;) {
            const size_t chunk_frames = std::min<size_t>(remaining, frames_per_buffer);
            const size_t written = stream->write_interleaved(write_frames.data(), chunk_frames);
            if (written == 0) {
                state.SkipWithError("failed to prefill AudioStream (threaded)");
                return;
            }
            remaining -= written;
        }

        BenchAudioDevice device;
        device.configure(make_device_properties(Lowl::Audio::SampleFormat::FLOAT_32), stream, frames_per_buffer);

        std::atomic<bool> producer_running{true};
        std::thread producer([&stream, &write_frames, &producer_running, frames_per_buffer]() {
            while (producer_running.load(std::memory_order_relaxed)) {
                const size_t written = stream->write_interleaved(write_frames.data(), frames_per_buffer);
                if (written == 0) {
                    std::this_thread::yield();
                }
            }
        });

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        for (auto _ : state) {
            for (size_t callback_index = 0; callback_index < kCallbacksPerIteration; callback_index++) {
                benchmark::DoNotOptimize(buffer.data());
                device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
                benchmark::ClobberMemory();
            }
        }

        producer_running.store(false, std::memory_order_relaxed);
        producer.join();

        state.SetItemsProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(kCallbacksPerIteration));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer) *
                                static_cast<int64_t>(kCallbacksPerIteration) * static_cast<int64_t>(bytes_per_frame));
    }

    static void BM_AudioDevice_Render_FLOAT32_Stereo(benchmark::State &state) {
        bench_audio_device_render<float>(state, Lowl::Audio::SampleFormat::FLOAT_32);
    }

    static void BM_AudioDevice_Render_INT16_Stereo(benchmark::State &state) {
        bench_audio_device_render<int16_t>(state, Lowl::Audio::SampleFormat::INT_16);
    }

    static void BM_AudioDevice_Render_INT24_Stereo(benchmark::State &state) {
        bench_audio_device_render_format(state, Lowl::Audio::SampleFormat::INT_24, 3);
    }

    static void BM_AudioDevice_Render_INT32_Stereo(benchmark::State &state) {
        bench_audio_device_render<int32_t>(state, Lowl::Audio::SampleFormat::INT_32);
    }

    static void BM_AudioDevice_Render_FLOAT32_Mono(benchmark::State &state) {
        const unsigned long frames_per_buffer = static_cast<unsigned long>(state.range(0));
        const unsigned long bytes_per_frame = sizeof(float) * kMonoChannelCount;

        auto source = std::make_shared<ConstantMonoSource>();
        BenchAudioDevice device;
        device.configure(
            make_device_properties(Lowl::Audio::SampleFormat::FLOAT_32, Lowl::Audio::ChannelLayout::Mono),
            source, frames_per_buffer);

        std::vector<std::byte> buffer(static_cast<size_t>(frames_per_buffer) * bytes_per_frame);
        for (auto _ : state) {
            benchmark::DoNotOptimize(buffer.data());
            device.write(buffer.data(), frames_per_buffer, bytes_per_frame);
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(frames_per_buffer));
        state.SetBytesProcessed(
            state.iterations() * static_cast<int64_t>(frames_per_buffer) * static_cast<int64_t>(bytes_per_frame));
    }

    static void BM_AudioDevice_Render_AudioSpace(benchmark::State &state) {
        bench_audio_device_render_audio_space(state);
    }

    static void BM_AudioDevice_Render_AudioStream(benchmark::State &state) {
        bench_audio_device_render_audio_stream(state);
    }

    static void BM_AudioMixer_Render(benchmark::State &state) {
        bench_audio_mixer_render(state);
    }

    static void BM_AudioDevice_Render_AudioSpace_Mono(benchmark::State &state) {
        bench_audio_device_render_audio_space_mono(state);
    }

    static void BM_AudioDevice_Render_AudioStream_Planar(benchmark::State &state) {
        bench_audio_device_render_audio_stream_planar(state);
    }

    static void BM_AudioDevice_Render_AudioStream_Threaded(benchmark::State &state) {
        bench_audio_device_render_audio_stream_threaded(state);
    }
} // namespace

BENCHMARK(BM_AudioDevice_Render_FLOAT32_Stereo)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_INT16_Stereo)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_INT24_Stereo)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_INT32_Stereo)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_FLOAT32_Mono)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioMixer_Render)
    ->Apply(apply_voice_and_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_AudioSpace)
    ->Apply(apply_voice_and_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_AudioSpace_Mono)
    ->Apply(apply_voice_and_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_AudioStream)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_AudioStream_Planar)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_AudioDevice_Render_AudioStream_Threaded)
    ->Apply(apply_frame_args)
    ->Unit(benchmark::kMicrosecond);
