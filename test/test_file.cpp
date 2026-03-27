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
    SUBCASE("File - opening a missing file reports an error") {
        Lowl::File file;
        Lowl::Error error;
        file.open("/tmp/lowl_audio_missing_file_that_should_not_exist", error);

        REQUIRE(error.has_error());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::FileStreamOpenFailed);
    }

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

    SUBCASE("File - seek and get_position track the cursor") {
        ScopedTempFile temp_file("abcdef");

        Lowl::File file;
        Lowl::Error error;
        file.open(temp_file.get_path(), error);

        REQUIRE_FALSE(error.has_error());
        REQUIRE(file.seek(2));

        size_t position = 0;
        REQUIRE(file.get_position(position));
        REQUIRE_EQ(position, 2U);

        size_t length = 2;
        std::unique_ptr<uint8_t[]> data = file.read_buffer(length);
        REQUIRE_EQ(length, 2U);
        REQUIRE_EQ(static_cast<char>(data[0]), 'c');
        REQUIRE_EQ(static_cast<char>(data[1]), 'd');
    }
}
