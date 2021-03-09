#ifndef LOWL_FILE_H
#define LOWL_FILE_H

#include <string>
#include "lowl_error.h"

namespace Lowl {

    class LowlFile
    {
    private:
        void* user_data;
        std::string path;

    public:
        void open(const std::string& p_path, LowlError &error);
        void close();
        void seek(size_t p_position);
        size_t get_position() const;
        size_t get_length() const;
        uint8_t read_u8() const;
        int get_buffer(uint8_t* p_dst, int p_length) const;
        bool is_eof() const;
        std::string get_path();
        LowlFile();
        ~LowlFile();
    };
}

#endif