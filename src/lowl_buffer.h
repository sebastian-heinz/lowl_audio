#ifndef LOWL_BUFFER_H
#define LOWL_BUFFER_H

#include <cstddef>
#include <cstdint>

namespace Lowl {
    class Buffer {

    private:
        size_t position;
        size_t virtual_length;
        size_t real_length;
        uint8_t *data;

        void grow(size_t p_length);

    public:
        void write_data(void *p_src, size_t p_length);

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

        size_t get_available() const;

        Buffer *slice(size_t p_length);

        Buffer(void *p_data, int p_length);

        Buffer();

        ~Buffer();
    };
}

#endif
