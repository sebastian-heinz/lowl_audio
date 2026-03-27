#include <doctest/doctest.h>

#include "lowl_file.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>

namespace {
    class ScopedTempFile {
    private:
        std::string path;

    public:
        explicit ScopedTempFile(const std::string &p_contents) {
            char temp_path[] = "/tmp/lowl_audio_test_XXXXXX";
            const int fd = mkstemp(temp_path);
            REQUIRE(fd >= 0);
            close(fd);
            path = temp_path;

            std::ofstream out(path, std::ios::binary);
            out.write(p_contents.data(), static_cast<std::streamsize>(p_contents.size()));
        }

        ~ScopedTempFile() {
            if (!path.empty()) {
                std::remove(path.c_str());
            }
        }

        const std::string &get_path() const {
            return path;
        }
    };
} // namespace

TEST_CASE("File") {
    SUBCASE("File - unopened file reports eof and empty reads") {
        Lowl::File file;
        size_t length = 4;
        std::unique_ptr<uint8_t[]> data = file.read_buffer(length);

        REQUIRE(data != nullptr);
        REQUIRE_EQ(length, 0U);
        REQUIRE(file.is_eof());
    }

    SUBCASE("File - short read preserves bytes read at eof") {
        ScopedTempFile temp_file("abc");

        Lowl::File file;
        Lowl::Error error;
        file.open(temp_file.get_path(), error);

        REQUIRE_FALSE(error.has_error());

        size_t length = 5;
        std::unique_ptr<uint8_t[]> data = file.read_buffer(length);

        REQUIRE(data != nullptr);
        REQUIRE_EQ(length, 3U);
        REQUIRE_EQ(static_cast<char>(data[0]), 'a');
        REQUIRE_EQ(static_cast<char>(data[1]), 'b');
        REQUIRE_EQ(static_cast<char>(data[2]), 'c');
        REQUIRE(file.is_eof());
    }
}
