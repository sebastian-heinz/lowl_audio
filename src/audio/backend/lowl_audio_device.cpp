#include "lowl_audio_device.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <tuple>

#include "audio/convert/lowl_audio_sample_converter.h"
#include "audio/simd/lowl_audio_simd.h"
#include "lowl_logger.h"

namespace {
    auto make_property_score(const Lowl::Audio::AudioDeviceProperties &p_requested,
                             const Lowl::Audio::AudioDeviceProperties &p_candidate) {
        const bool layout_mismatch =
            p_requested.audio_format.channel_layout.is_valid() &&
            p_candidate.audio_format.channel_layout != p_requested.audio_format.channel_layout;
        const bool format_mismatch = p_requested.sample_format != Lowl::Audio::SampleFormat::Unknown &&
                                     p_candidate.sample_format != p_requested.sample_format;
        const double sample_rate_distance =
            p_requested.audio_format.sample_rate > Lowl::NO_SAMPLE_RATE &&
                    !Lowl::Audio::sample_rates_equal(p_requested.audio_format.sample_rate,
                                                     p_candidate.audio_format.sample_rate)
                ? std::abs(p_candidate.audio_format.sample_rate - p_requested.audio_format.sample_rate)
                : 0.0;

        return std::make_tuple(!p_candidate.is_supported,
                               layout_mismatch,
                               format_mismatch,
                               p_candidate.exclusive_mode != p_requested.exclusive_mode,
                               sample_rate_distance);
    }

    void write_interleaved_float32(const Lowl::Audio::AudioBlockView &p_block, void *p_dst, const uint32_t p_frames) {
        auto *dst = static_cast<float *>(p_dst);
        if (p_block.channel_count == 1) {
#if defined(LOWL_TYPE_SAMPLE_64)
            const Lowl::Sample *src = p_block.channel(0);
            for (uint32_t frame_index = 0; frame_index < p_frames; frame_index++) {
                dst[frame_index] = Lowl::Audio::SampleConverter::sample_to_float(src[frame_index]);
            }
#else
            std::memcpy(dst, p_block.channel(0), static_cast<size_t>(p_frames) * sizeof(float));
#endif
            return;
        }
        if (p_block.channel_count == 2) {
            const Lowl::Sample *src_l = p_block.channel(0);
            const Lowl::Sample *src_r = p_block.channel(1);
            Lowl::Audio::Simd::dispatch().interleave_stereo_float32(src_l, src_r, dst, p_frames);
            return;
        }

        const uint8_t channel_count = p_block.channel_count;
        for (uint32_t frame_index = 0; frame_index < p_frames; frame_index++) {
            for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                dst[static_cast<size_t>(frame_index) * channel_count + channel_index] =
                    Lowl::Audio::SampleConverter::sample_to_float(
                        p_block.channel(channel_index)[frame_index]);
            }
        }
    }

    void write_interleaved_int16(const Lowl::Audio::AudioBlockView &p_block, void *p_dst, const uint32_t p_frames) {
        auto *dst = static_cast<int16_t *>(p_dst);
        if (p_block.channel_count == 2) {
            const Lowl::Sample *src_l = p_block.channel(0);
            const Lowl::Sample *src_r = p_block.channel(1);
            Lowl::Audio::Simd::dispatch().interleave_stereo_int16(src_l, src_r, dst, p_frames);
            return;
        }
        const uint8_t channel_count = p_block.channel_count;
        for (uint32_t frame_index = 0; frame_index < p_frames; frame_index++) {
            for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
                dst[static_cast<size_t>(frame_index) * channel_count + channel_index] =
                    Lowl::Audio::SampleConverter::sample_to_int16(p_block.channel(channel_index)[frame_index]);
            }
        }
    }

    bool write_interleaved_generic(const Lowl::Audio::SampleFormat p_format,
                                   const Lowl::Audio::AudioBlockView &p_block,
                                   void *p_dst,
                                   const uint32_t p_frames) {
        void *write_ptr = p_dst;
        for (uint32_t frame_index = 0; frame_index < p_frames; frame_index++) {
            for (uint8_t channel_index = 0; channel_index < p_block.channel_count; channel_index++) {
                if (!Lowl::Audio::SampleConverter::write_sample(
                        p_format, p_block.channel(channel_index)[frame_index], &write_ptr)) {
                    return false;
                }
            }
        }
        return true;
    }

    int get_silence_fill_value(const Lowl::Audio::SampleFormat p_format) {
        return p_format == Lowl::Audio::SampleFormat::U_INT_8 ? 0x80 : 0x00;
    }

    void fill_device_silence(void *p_dst,
                             const size_t p_byte_count,
                             const Lowl::Audio::SampleFormat p_format) {
        if (p_dst == nullptr || p_byte_count == 0) {
            return;
        }
        std::memset(p_dst, get_silence_fill_value(p_format), p_byte_count);
    }

    void fill_planar_device_silence(void *const *p_dst_channels,
                                    const size_t *p_dst_byte_sizes,
                                    const uint8_t p_channel_count,
                                    const size_t p_requested_bytes_per_channel,
                                    const Lowl::Audio::SampleFormat p_format) {
        if (p_dst_channels == nullptr || p_dst_byte_sizes == nullptr) {
            return;
        }
        for (uint8_t channel_index = 0; channel_index < p_channel_count; channel_index++) {
            fill_device_silence(p_dst_channels[channel_index],
                                std::min(p_dst_byte_sizes[channel_index], p_requested_bytes_per_channel),
                                p_format);
        }
    }
} // namespace

std::string Lowl::Audio::AudioDevice::get_name() const {
    return name;
}

void Lowl::Audio::AudioDevice::set_name(const std::string &p_name) {
    name = p_name;
}

Lowl::Audio::AudioDevice::AudioDevice(_constructor_tag) {
    properties_list = std::vector<AudioDeviceProperties>();
    name = std::string();
    audio_source = std::shared_ptr<AudioSource>();
    audio_device_properties = AudioDeviceProperties{};
}

Lowl::Audio::AudioDeviceProperties
Lowl::Audio::AudioDevice::get_closest_properties(AudioDeviceProperties p_audio_device_properties, Error &error) const {
    if (properties_list.empty()) {
        error.set_error(ErrorCode::DeviceHasNoAudioProperties);
        return AudioDeviceProperties{};
    }

    auto best_property = properties_list.begin();
    auto best_score = make_property_score(p_audio_device_properties, *best_property);
    for (auto property = std::next(properties_list.begin()); property != properties_list.end(); ++property) {
        const auto property_score = make_property_score(p_audio_device_properties, *property);
        if (property_score < best_score) {
            best_property = property;
            best_score = property_score;
        }
    }
    return *best_property;
}

std::vector<Lowl::Audio::AudioDeviceProperties> Lowl::Audio::AudioDevice::get_properties_list() const {
    return properties_list;
}

Lowl::Audio::AudioDevice::~AudioDevice() {
    unpublish_render_state();
}

bool Lowl::Audio::AudioDevice::allocate_render_buffer(const unsigned long p_frame_capacity) {
    if (published_render_state.load(std::memory_order_acquire) != nullptr || render_state_owner != nullptr) {
        LOWL_LOG_ERROR("AudioDevice::allocate_render_buffer: existing render state must be retired first.");
        assert(false && "Existing render state must be retired before replacement");
        return false;
    }

    auto next_render_state = std::make_unique<RenderState>();
    next_render_state->audio_device_properties = audio_device_properties;
    next_render_state->audio_source = audio_source;
    const uint8_t channel_count = audio_device_properties.audio_format.channel_layout.channel_count;
    next_render_state->render_buffer = AudioBuffer(static_cast<uint32_t>(p_frame_capacity), channel_count);

    render_state_owner = std::move(next_render_state);
    published_render_state.store(render_state_owner.get(), std::memory_order_release);
    return true;
}

Lowl::Audio::AudioDevice::RenderState *Lowl::Audio::AudioDevice::load_render_state() const {
    return published_render_state.load(std::memory_order_acquire);
}

void Lowl::Audio::AudioDevice::unpublish_render_state() {
    published_render_state.store(nullptr, std::memory_order_release);
}

bool Lowl::Audio::AudioDevice::release_render_state() {
    if (published_render_state.load(std::memory_order_acquire) != nullptr) {
        LOWL_LOG_ERROR("AudioDevice::release_render_state: render state is still published.");
        assert(false && "Cannot release a published render state");
        return false;
    }
    render_state_owner.reset();
    return true;
}

void Lowl::Audio::AudioDevice::render_to_device_buffer(RenderState *p_render_state,
                                                       void *p_dst,
                                                       size_t p_dst_byte_size,
                                                       unsigned long p_frames_per_buffer,
                                                       unsigned long p_bytes_per_frame) {
    if (p_dst == nullptr || p_dst_byte_size == 0) {
        return;
    }

    const SampleFormat published_sample_format =
        p_render_state != nullptr ? p_render_state->audio_device_properties.sample_format : SampleFormat::Unknown;
    const size_t requested_total_bytes = static_cast<size_t>(p_frames_per_buffer) * p_bytes_per_frame;
    const size_t total_bytes = std::min(requested_total_bytes, p_dst_byte_size);
    if (requested_total_bytes > p_dst_byte_size) {
        fill_device_silence(p_dst, p_dst_byte_size, published_sample_format);
        return;
    }

    if (!p_render_state) {
        fill_device_silence(p_dst, total_bytes, SampleFormat::Unknown);
        return;
    }

    const AudioDeviceProperties &published_properties = p_render_state->audio_device_properties;
    const size_t sample_size_bytes = get_sample_size_bytes(published_properties.sample_format);
    if (sample_size_bytes == 0) {
        fill_device_silence(p_dst, total_bytes, published_properties.sample_format);
        return;
    }

    if (!p_render_state->audio_source) {
        fill_device_silence(p_dst, total_bytes, published_properties.sample_format);
        return;
    }

    const size_t expected_bytes_per_frame =
        sample_size_bytes * static_cast<size_t>(published_properties.audio_format.channel_layout.channel_count);
    if (expected_bytes_per_frame == 0 || expected_bytes_per_frame != p_bytes_per_frame) {
        fill_device_silence(p_dst, total_bytes, published_properties.sample_format);
        return;
    }

    AudioBlockView output_block = p_render_state->render_buffer.view(static_cast<uint32_t>(p_frames_per_buffer));
    p_render_state->render_buffer.clear(output_block.frame_count);
    AudioSource::MixGainVector unity_gain;
    const AudioSource::RenderResult render_result =
        p_render_state->audio_source->mix_into(output_block, unity_gain);
    const uint32_t produced_frames = std::min(render_result.frames_produced, output_block.frame_count);

    switch (published_properties.sample_format) {
        case SampleFormat::FLOAT_32:
            write_interleaved_float32(output_block, p_dst, produced_frames);
            break;
        case SampleFormat::FLOAT_64:
        case SampleFormat::INT_32:
        case SampleFormat::INT_24:
        case SampleFormat::INT_16:
        case SampleFormat::INT_8:
        case SampleFormat::U_INT_8:
        case SampleFormat::Unknown:
            if (published_properties.sample_format == SampleFormat::INT_16) {
                write_interleaved_int16(output_block, p_dst, produced_frames);
                break;
            }
            if (!write_interleaved_generic(published_properties.sample_format, output_block, p_dst, produced_frames)) {
                fill_device_silence(p_dst, total_bytes, published_properties.sample_format);
                return;
            }
            break;
    }

    const size_t bytes_written = static_cast<size_t>(produced_frames) * expected_bytes_per_frame;
    if (bytes_written < total_bytes) {
        fill_device_silence(static_cast<uint8_t *>(p_dst) + bytes_written,
                            total_bytes - bytes_written,
                            published_properties.sample_format);
    }
}

bool Lowl::Audio::AudioDevice::render_to_planar_device_buffers(RenderState *p_render_state,
                                                               void *const *p_dst_channels,
                                                               const size_t *p_dst_byte_sizes,
                                                               const uint8_t p_dst_channel_count,
                                                               const unsigned long p_frames_per_buffer) {
    if (p_dst_channels == nullptr || p_dst_byte_sizes == nullptr || p_dst_channel_count == 0 ||
        p_dst_channel_count > AudioBlockView::MAX_CHANNELS) {
        return false;
    }

    if (p_render_state == nullptr) {
        fill_planar_device_silence(p_dst_channels,
                                   p_dst_byte_sizes,
                                   p_dst_channel_count,
                                   std::numeric_limits<size_t>::max(),
                                   SampleFormat::Unknown);
        return true;
    }

    const AudioDeviceProperties &published_properties = p_render_state->audio_device_properties;
    const SampleFormat sample_format = published_properties.sample_format;
    const size_t sample_size_bytes = get_sample_size_bytes(sample_format);
    const uint8_t channel_count = published_properties.audio_format.channel_layout.channel_count;
    if (sample_size_bytes == 0 || channel_count == 0 || channel_count > AudioBlockView::MAX_CHANNELS ||
        p_dst_channel_count != channel_count || p_frames_per_buffer > std::numeric_limits<uint32_t>::max() ||
        static_cast<size_t>(p_frames_per_buffer) > std::numeric_limits<size_t>::max() / sample_size_bytes) {
        fill_planar_device_silence(p_dst_channels,
                                   p_dst_byte_sizes,
                                   p_dst_channel_count,
                                   std::numeric_limits<size_t>::max(),
                                   sample_format);
        return false;
    }

    const size_t requested_bytes_per_channel = static_cast<size_t>(p_frames_per_buffer) * sample_size_bytes;
    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        if (p_dst_channels[channel_index] == nullptr ||
            p_dst_byte_sizes[channel_index] < requested_bytes_per_channel) {
            fill_planar_device_silence(p_dst_channels,
                                       p_dst_byte_sizes,
                                       channel_count,
                                       requested_bytes_per_channel,
                                       sample_format);
            return false;
        }
    }

    fill_planar_device_silence(p_dst_channels,
                               p_dst_byte_sizes,
                               channel_count,
                               requested_bytes_per_channel,
                               sample_format);
    if (!p_render_state->audio_source) {
        return true;
    }

#if defined(LOWL_TYPE_SAMPLE_64)
    const bool can_render_directly = sample_format == SampleFormat::FLOAT_64;
#else
    const bool can_render_directly = sample_format == SampleFormat::FLOAT_32;
#endif
    if (can_render_directly) {
        AudioBlockView output_block{};
        output_block.frame_count = static_cast<uint32_t>(p_frames_per_buffer);
        output_block.channel_count = channel_count;
        for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
            output_block.channels[static_cast<size_t>(channel_index)] =
                static_cast<Sample *>(p_dst_channels[channel_index]);
        }

        AudioSource::MixGainVector unity_gain;
        p_render_state->audio_source->mix_into(output_block, unity_gain);
        return true;
    }

    AudioBlockView output_block =
        p_render_state->render_buffer.view(static_cast<uint32_t>(p_frames_per_buffer));
    if (output_block.channel_count != channel_count) {
        return false;
    }
    p_render_state->render_buffer.clear(output_block.frame_count);
    AudioSource::MixGainVector unity_gain;
    const AudioSource::RenderResult render_result =
        p_render_state->audio_source->mix_into(output_block, unity_gain);
    const uint32_t produced_frames = std::min(render_result.frames_produced, output_block.frame_count);

    for (uint8_t channel_index = 0; channel_index < channel_count; channel_index++) {
        void *write_ptr = p_dst_channels[channel_index];
        const Sample *src = output_block.channel(channel_index);
        for (uint32_t frame_index = 0; frame_index < produced_frames; frame_index++) {
            if (!SampleConverter::write_sample(sample_format, src[frame_index], &write_ptr)) {
                fill_planar_device_silence(p_dst_channels,
                                           p_dst_byte_sizes,
                                           channel_count,
                                           requested_bytes_per_channel,
                                           sample_format);
                return false;
            }
        }
    }
    return true;
}
