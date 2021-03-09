#include "lowl_buffer.h"

#include <stdlib.h>
#include <cstring>

// Swap 16, 32 and 64 bits value for endianness.
#if defined(__GNUC__)
#define BSWAP16(x) __builtin_bswap16(x)
#define BSWAP32(x) __builtin_bswap32(x)
#define BSWAP64(x) __builtin_bswap64(x)
#else
static inline uint16_t BSWAP16(uint16_t x) {
    return (x >> 8) | (x << 8);
}

static inline uint32_t BSWAP32(uint32_t x) {
    return ((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24));
}

static inline uint64_t BSWAP64(uint64_t x) {
    x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
    x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
    x = (x & 0x00FF00FF00FF00FF) << 8 | (x & 0xFF00FF00FF00FF00) >> 8;
    return x;
}
#endif

const size_t GROW_SIZE = 1024;

void Lowl::Buffer::write_data(void *p_src, size_t p_length) {
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
    uint16_t value = (read_u8() | read_u8() << 8);
    return value;
}

uint32_t Lowl::Buffer::read_u32() {
    uint32_t value = read_u8() | (read_u8() << 8) | (read_u8() << 16) | (read_u8() << 24);
    return value;
}

void Lowl::Buffer::read_data(void *p_dst, size_t p_length) {
    if (p_length < 0) {
        return;
    }
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
    if (p_src_offset < 0) {
        return;
    }
    if (p_src_count < 0) {
        return;
    }
    if (p_dst_length < 0) {
        return;
    }
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
    //void * memcpy ( void * destination, const void * source, size_t num );
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

size_t Lowl::Buffer::get_available() const {
    return virtual_length - position;
}

Lowl::Buffer *Lowl::Buffer::slice(size_t p_length) {
    Buffer *buffer = new Buffer(&data[position], p_length);
    return buffer;
}

void Lowl::Buffer::grow(size_t p_length) {
    int new_real_length = real_length + p_length;
    void *newloc = realloc(data, new_real_length);
    if (!newloc) {
        return;
    }
    data = (uint8_t *) newloc;
    real_length = new_real_length;
}

Lowl::Buffer::Buffer(void *p_data, int p_length) {
    real_length = p_length;
    data = (uint8_t *) malloc(real_length);
    position = 0;
    virtual_length = 0;
    write_data(p_data, p_length);
}

Lowl::Buffer::Buffer() {
    real_length = GROW_SIZE;
    data = (uint8_t *) malloc(real_length);
    position = 0;
    virtual_length = 0;
}

Lowl::Buffer::~Buffer() {
    free(data);
}

