#include "lowl_buffer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {
    uint8_t *allocate_buffer(const size_t p_length) {
        if (p_length == 0) {
            return nullptr;
        }
        void *storage = std::malloc(p_length);
        if (!storage) {
            std::abort();
        }
        return static_cast<uint8_t *>(storage);
    }
} // namespace

constexpr size_t GROW_SIZE = 1024;

void Lowl::Buffer::write_data(const void *p_src, size_t p_length) {
    if (p_length <= 0) {
        return;
    }
    if (position + p_length > real_length) {
        grow(p_length);
    }
    memcpy(&data[position], p_src, p_length);
    position += p_length;
    if (position > virtual_length) {
        virtual_length = position;
    }
}

uint8_t Lowl::Buffer::read_u8() {
    if (position >= virtual_length) {
        // end of file
        return 0;
    }
    uint8_t value = data[position];
    position++;
    return value;
}

uint16_t Lowl::Buffer::read_u16() {
    const uint8_t b0 = read_u8();
    const uint8_t b1 = read_u8();
    if (endianness == Endianness::Big) {
        return static_cast<uint16_t>((static_cast<uint16_t>(b0) << 8) | b1);
    }
    return static_cast<uint16_t>(b0 | (static_cast<uint16_t>(b1) << 8));
}

uint32_t Lowl::Buffer::read_u32() {
    const uint8_t b0 = read_u8();
    const uint8_t b1 = read_u8();
    const uint8_t b2 = read_u8();
    const uint8_t b3 = read_u8();
    if (endianness == Endianness::Big) {
        return (static_cast<uint32_t>(b0) << 24) | (static_cast<uint32_t>(b1) << 16) |
               (static_cast<uint32_t>(b2) << 8) | static_cast<uint32_t>(b3);
    }
    return static_cast<uint32_t>(b0) | (static_cast<uint32_t>(b1) << 8) | (static_cast<uint32_t>(b2) << 16) |
           (static_cast<uint32_t>(b3) << 24);
}

void Lowl::Buffer::read_data(void *p_dst, size_t p_length) {
    if (p_dst == nullptr) {
        return;
    }
    if (p_length > virtual_length) {
        return;
    }
    if (position + p_length > virtual_length) {
        return;
    }
    memcpy(p_dst, &data[position], p_length);
    position += p_length;
}

void Lowl::Buffer::get_data(size_t p_src_offset, size_t p_src_count, void *p_dst, size_t p_dst_length) const {
    if (p_dst == nullptr) {
        return;
    }
    if (p_src_count > virtual_length) {
        return;
    }
    if (p_src_offset + p_src_count > virtual_length) {
        return;
    }
    if (p_dst_length < p_src_count) {
        return;
    }
    memcpy(p_dst, &data[p_src_offset], p_src_count);
}

void Lowl::Buffer::get_all_data(void *p_dst, size_t p_dst_length) const {
    if (p_dst_length <= 0) {
        return;
    }
    if (p_dst_length < virtual_length) {
        return;
    }
    if (p_dst == nullptr) {
        return;
    }
    memcpy(p_dst, &data[0], virtual_length);
}

void Lowl::Buffer::seek(size_t p_position) {
    if (p_position > virtual_length) {
        position = virtual_length;
        return;
    }
    position = p_position;
}

size_t Lowl::Buffer::get_position() const {
    return position;
}

size_t Lowl::Buffer::get_length() const {
    return virtual_length;
}

void Lowl::Buffer::set_length(size_t p_length) {
    if (p_length > real_length) {
        grow(p_length - real_length);
    }
    virtual_length = p_length;
}

void Lowl::Buffer::set_endianness(const Endianness p_endianness) {
    endianness = p_endianness;
}

Lowl::Buffer::Endianness Lowl::Buffer::get_endianness() const {
    return endianness;
}

size_t Lowl::Buffer::get_available() const {
    return virtual_length - position;
}

std::unique_ptr<Lowl::Buffer> Lowl::Buffer::slice(size_t p_length) const {
    const size_t available = position <= virtual_length ? virtual_length - position : 0;
    const size_t slice_length = std::min(p_length, available);
    const void *slice_data = (data != nullptr && slice_length > 0) ? static_cast<const void *>(data + position) : nullptr;
    return std::make_unique<Buffer>(slice_data, slice_length, endianness);
}

void Lowl::Buffer::grow(const size_t p_length) {
    size_t new_real_length = real_length + p_length;
    void *newloc = realloc(data, new_real_length);
    if (!newloc) {
        std::abort();
    }
    data = static_cast<uint8_t *>(newloc);
    real_length = new_real_length;
}

Lowl::Buffer::Buffer(const void *p_data, const size_t p_length, const Endianness p_endianness) {
    real_length = p_length;
    data = allocate_buffer(real_length);
    position = 0;
    virtual_length = 0;
    endianness = p_endianness;
    if (p_length == 0) {
        return;
    }
    if (p_data == nullptr) {
        std::memset(data, 0, p_length);
        virtual_length = p_length;
        return;
    }
    write_data(p_data, p_length);
}

Lowl::Buffer::Buffer(const Endianness p_endianness) {
    real_length = GROW_SIZE;
    data = allocate_buffer(real_length);
    position = 0;
    virtual_length = 0;
    endianness = p_endianness;
}

Lowl::Buffer::Buffer(Buffer &&p_other) noexcept
    : position(p_other.position),
      virtual_length(p_other.virtual_length),
      real_length(p_other.real_length),
      data(p_other.data),
      endianness(p_other.endianness) {
    p_other.position = 0;
    p_other.virtual_length = 0;
    p_other.real_length = 0;
    p_other.data = nullptr;
    p_other.endianness = Endianness::Little;
}

Lowl::Buffer &Lowl::Buffer::operator=(Buffer &&p_other) noexcept {
    if (this == &p_other) {
        return *this;
    }

    std::free(data);
    position = p_other.position;
    virtual_length = p_other.virtual_length;
    real_length = p_other.real_length;
    data = p_other.data;
    endianness = p_other.endianness;

    p_other.position = 0;
    p_other.virtual_length = 0;
    p_other.real_length = 0;
    p_other.data = nullptr;
    p_other.endianness = Endianness::Little;
    return *this;
}

Lowl::Buffer::~Buffer() {
    std::free(data);
}
