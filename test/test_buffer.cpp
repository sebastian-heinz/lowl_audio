#include <doctest/doctest.h>

#include "lowl_buffer.h"

#include <cstdint>
#include <memory>
#include <utility>

TEST_CASE("Buffer") {
    SUBCASE("Buffer - reads little endian values by default") {
        const uint8_t bytes[] = {0x34, 0x12, 0x78, 0x56, 0x34, 0x12};
        Lowl::Buffer buffer(bytes, sizeof(bytes));
        buffer.seek(0);

        REQUIRE_EQ(buffer.read_u16(), 0x1234U);
        REQUIRE_EQ(buffer.read_u32(), 0x12345678U);
    }

    SUBCASE("Buffer - can read big endian values") {
        const uint8_t bytes[] = {0x12, 0x34, 0x12, 0x34, 0x56, 0x78};
        Lowl::Buffer buffer(bytes, sizeof(bytes), Lowl::Buffer::Endianness::Big);
        buffer.seek(0);

        REQUIRE_EQ(buffer.read_u16(), 0x1234U);
        REQUIRE_EQ(buffer.read_u32(), 0x12345678U);
    }

    SUBCASE("Buffer - slice returns owned clamped copy") {
        const uint8_t bytes[] = {1, 2, 3};
        Lowl::Buffer buffer(bytes, 3);
        buffer.seek(0);

        REQUIRE_EQ(buffer.read_u8(), 1);
        std::unique_ptr<Lowl::Buffer> slice = buffer.slice(10);

        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_length(), 2U);
        slice->seek(0);
        REQUIRE_EQ(slice->read_u8(), 2U);
        REQUIRE_EQ(slice->read_u8(), 3U);
    }

    SUBCASE("Buffer - move operations transfer ownership and contents") {
        const uint8_t bytes[] = {4, 5, 6};
        Lowl::Buffer original(bytes, 3, Lowl::Buffer::Endianness::Big);
        original.seek(0);
        Lowl::Buffer moved(std::move(original));

        REQUIRE_EQ(moved.get_length(), 3U);
        REQUIRE_EQ(moved.get_endianness(), Lowl::Buffer::Endianness::Big);
        REQUIRE_EQ(moved.read_u8(), 4U);

        Lowl::Buffer assigned;
        assigned = std::move(moved);
        assigned.seek(1);

        REQUIRE_EQ(assigned.get_length(), 3U);
        REQUIRE_EQ(assigned.get_endianness(), Lowl::Buffer::Endianness::Big);
        REQUIRE_EQ(assigned.read_u8(), 5U);
        REQUIRE_EQ(assigned.read_u8(), 6U);
    }

    SUBCASE("Buffer - slice preserves endianness") {
        const uint8_t bytes[] = {0xFF, 0x12, 0x34};
        Lowl::Buffer buffer(bytes, sizeof(bytes), Lowl::Buffer::Endianness::Big);
        buffer.seek(1);

        std::unique_ptr<Lowl::Buffer> slice = buffer.slice(2);
        REQUIRE(slice != nullptr);
        REQUIRE_EQ(slice->get_endianness(), Lowl::Buffer::Endianness::Big);
        slice->seek(0);
        REQUIRE_EQ(slice->read_u16(), 0x1234U);
    }
}
