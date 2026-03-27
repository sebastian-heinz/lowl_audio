#include <doctest/doctest.h>

#include "lowl_error.h"
#include "lowl_release_pool.h"
#include "lowl_timer.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
    template <typename Predicate>
    bool wait_until(Predicate p_predicate, const std::chrono::milliseconds p_timeout) {
        const auto deadline = std::chrono::steady_clock::now() + p_timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (p_predicate()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return p_predicate();
    }

    struct TrackedObject {
        std::atomic<int> *destructions = nullptr;

        explicit TrackedObject(std::atomic<int> *p_destructions)
            : destructions(p_destructions) {
        }

        ~TrackedObject() {
            if (destructions != nullptr) {
                destructions->fetch_add(1, std::memory_order_relaxed);
            }
        }
    };
} // namespace

TEST_CASE("Error") {
    SUBCASE("Error - set_error and clear reset the vendor state") {
        Lowl::Error error;

        REQUIRE(error.ok());
        REQUIRE_FALSE(error.has_error());
        REQUIRE_FALSE(error.has_vendor_error());
        REQUIRE_EQ(error.get_error_code(), static_cast<int>(Lowl::ErrorCode::NoError));
        REQUIRE_EQ(error.get_error_text(), std::string("NoError"));

        error.set_error(Lowl::ErrorCode::ReaderNoAudioData);
        REQUIRE(error.has_error());
        REQUIRE_FALSE(error.has_vendor_error());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::ReaderNoAudioData);
        REQUIRE_EQ(error.get_error_code(), static_cast<int>(Lowl::ErrorCode::ReaderNoAudioData));
        REQUIRE_EQ(error.get_error_text(), std::string("ReaderNoAudioData"));

        error.clear();
        REQUIRE(error.ok());
        REQUIRE_FALSE(error.has_vendor_error());
        REQUIRE_EQ(error.get_error_text(), std::string("NoError"));
    }

    SUBCASE("Error - vendor errors preserve the vendor code and text") {
        Lowl::Error error;
        error.set_vendor_error(1234, Lowl::Error::VendorError::OpusFileVendorError);

        REQUIRE(error.has_error());
        REQUIRE(error.has_vendor_error());
        REQUIRE_EQ(error.get_error(), Lowl::ErrorCode::OpusFileVendorError);
        REQUIRE_EQ(error.get_error_text(), std::string("OpusFileVendorError"));
        REQUIRE_EQ(error.get_vendor_error(), 1234L);
    }
}

TEST_CASE("Timer") {
    SUBCASE("Timer - single-shot callback fires once") {
        Lowl::Timer timer;
        std::atomic<int> callback_count{0};

        timer.start_timer(
            [&]() { callback_count.fetch_add(1, std::memory_order_relaxed); },
            std::chrono::milliseconds(5)
        );

        REQUIRE(wait_until([&]() { return callback_count.load(std::memory_order_relaxed) == 1; },
                           std::chrono::milliseconds(200)));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        REQUIRE_EQ(callback_count.load(std::memory_order_relaxed), 1);
    }

    SUBCASE("Timer - interval stop from callback does not deadlock and stops future callbacks") {
        Lowl::Timer timer;
        std::atomic<int> callback_count{0};

        timer.start_interval(
            [&]() {
                const int next = callback_count.fetch_add(1, std::memory_order_relaxed) + 1;
                if (next == 1) {
                    timer.stop();
                }
            },
            std::chrono::milliseconds(2)
        );

        REQUIRE(wait_until([&]() { return callback_count.load(std::memory_order_relaxed) >= 1; },
                           std::chrono::milliseconds(200)));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        REQUIRE_EQ(callback_count.load(std::memory_order_relaxed), 1);
    }

    SUBCASE("Timer - destructor stops an active interval thread") {
        std::atomic<int> callback_count{0};

        {
            Lowl::Timer timer;
            timer.start_interval(
                [&]() { callback_count.fetch_add(1, std::memory_order_relaxed); },
                std::chrono::milliseconds(2)
            );

            REQUIRE(wait_until([&]() { return callback_count.load(std::memory_order_relaxed) >= 1; },
                               std::chrono::milliseconds(200)));
        }

        const int count_after_destructor = callback_count.load(std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        REQUIRE_EQ(callback_count.load(std::memory_order_relaxed), count_after_destructor);
    }
}

TEST_CASE("ReleasePool") {
    SUBCASE("ReleasePool - released objects are destroyed after the timer sweep") {
        std::atomic<int> destructions{0};

        {
            Lowl::ReleasePool pool(std::chrono::milliseconds(5));
            std::shared_ptr<TrackedObject> object = std::make_shared<TrackedObject>(&destructions);
            pool.add(object);
            object.reset();

            REQUIRE(wait_until([&]() { return destructions.load(std::memory_order_relaxed) == 1; },
                               std::chrono::milliseconds(500)));
        }

        REQUIRE_EQ(destructions.load(std::memory_order_relaxed), 1);
    }

    SUBCASE("ReleasePool - duplicate and concurrent adds do not pin extra references") {
        std::atomic<int> destructions{0};

        {
            Lowl::ReleasePool pool(std::chrono::milliseconds(5));
            std::shared_ptr<TrackedObject> object = std::make_shared<TrackedObject>(&destructions);
            std::vector<std::thread> threads;

            for (int thread_index = 0; thread_index < 4; thread_index++) {
                threads.emplace_back([&]() {
                    for (int iteration = 0; iteration < 100; iteration++) {
                        pool.add(object);
                    }
                });
            }

            for (std::thread &thread : threads) {
                thread.join();
            }

            object.reset();
            REQUIRE(wait_until([&]() { return destructions.load(std::memory_order_relaxed) == 1; },
                               std::chrono::milliseconds(500)));
        }

        REQUIRE_EQ(destructions.load(std::memory_order_relaxed), 1);
    }
}
