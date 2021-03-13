#ifndef LOWL_FILE_H
#define LOWL_FILE_H

#include "lowl_error.h"

namespace Lowl {

    class LowlFile {
    private:
        void *user_data;
        std::string path;

    public:
        void open(const std::string &p_path, Error &error);

        void close();

        void seek(size_t p_position);

        size_t get_position() const;

        size_t get_length() const;

        uint8_t read_u8() const;

        /***
         * - read length-bytes from current cursor position
         * - store number of read bytes in length parameter
         */
        std::unique_ptr<uint8_t[]> read_buffer(size_t &length) const;

        bool is_eof() const;

        std::string get_path();

        LowlFile();

        ~LowlFile();
    };
}

#endif