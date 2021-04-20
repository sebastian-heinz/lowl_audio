#include "lowl_error.h"

#include <fstream>
#include <memory>

namespace Lowl {

    class File {
    private:
        std::unique_ptr<std::ifstream> file_stream;
        std::string path;
        size_t file_size;

    public:
        void open(const std::string &p_path, Error &error);

        void close();

        bool seek(size_t p_position);

        bool get_position(size_t &p_position) const;

        size_t get_length() const;

        uint8_t read_u8() const;

        /***
         * - read length-bytes from current cursor position
         * - store number of read bytes in length parameter
         */
        std::unique_ptr<uint8_t[]> read_buffer(size_t &length) const;

        bool is_eof() const;

        std::string get_path();

        File();

        ~File() = default;
    };
}

//#endif