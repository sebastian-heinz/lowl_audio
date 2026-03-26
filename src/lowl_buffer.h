#ifndef LOWL_BUFFER_H
#define LOWL_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Lowl {
    class Buffer {
    public:
        enum class Endianness : uint8_t {
            Little = 0,
            Big = 1,
        };

    private:
        size_t position = 0;
        size_t virtual_length = 0;
        size_t real_length = 0;
        uint8_t *data = nullptr;
        Endianness endianness = Endianness::Little;

        void grow(size_t p_length);

    public:
        void write_data(const void *p_src, size_t p_length);

        uint8_t read_u8();

        uint16_t read_u16();

        uint32_t read_u32();

        void read_data(void *p_dst, size_t p_length);

        void get_data(size_t p_src_offset, size_t p_src_count, void *p_dst, size_t p_dst_length) const;

        void get_all_data(void *p_dst, size_t p_dst_length) const;

        void seek(size_t p_position);

        size_t get_position() const;

        size_t get_length() const;

        void set_length(size_t p_length);

        void set_endianness(Endianness p_endianness);

        Endianness get_endianness() const;

        size_t get_available() const;

        std::unique_ptr<Buffer> slice(size_t p_length) const;

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;
        Buffer(Buffer &&p_other) noexcept;
        Buffer &operator=(Buffer &&p_other) noexcept;

        Buffer(const void *p_data, size_t p_length, Endianness p_endianness = Endianness::Little);

        Buffer(Endianness p_endianness = Endianness::Little);

        ~Buffer();
    };
} // namespace Lowl

#endif
