#include <doctest/doctest.h>

#include "lowl_logger.h"

#include <atomic>
#include <thread>
#include <vector>

namespace {
    struct ReceiverState {
        int expected_id = 0;
        std::atomic<int> calls{0};
        std::atomic<int> mismatches{0};
        std::atomic<int> formatting_failures{0};
    };

    void receiver_one(const Lowl::Logger::Log &p_log, void *p_user_data) {
        auto *state = static_cast<ReceiverState *>(p_user_data);
        if (state == nullptr || state->expected_id != 1) {
            if (state != nullptr) {
                state->mismatches.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }
        if (Lowl::Logger::format_log(p_log).empty()) {
            state->formatting_failures.fetch_add(1, std::memory_order_relaxed);
        }
        state->calls.fetch_add(1, std::memory_order_relaxed);
    }

    void receiver_two(const Lowl::Logger::Log &p_log, void *p_user_data) {
        auto *state = static_cast<ReceiverState *>(p_user_data);
        if (state == nullptr || state->expected_id != 2) {
            if (state != nullptr) {
                state->mismatches.fetch_add(1, std::memory_order_relaxed);
            }
            return;
        }
        if (Lowl::Logger::format_log(p_log).empty()) {
            state->formatting_failures.fetch_add(1, std::memory_order_relaxed);
        }
        state->calls.fetch_add(1, std::memory_order_relaxed);
    }
}

TEST_CASE("Logger") {
    SUBCASE("Logger - concurrent configuration keeps receiver and user data consistent") {
        ReceiverState state_one{};
        state_one.expected_id = 1;
        ReceiverState state_two{};
        state_two.expected_id = 2;

        Lowl::Logger::register_log_receiver(&receiver_one, &state_one);
        Lowl::Logger::set_log_level(Lowl::Logger::Level::Info);

        std::atomic<bool> start{false};
        std::vector<std::thread> threads;

        threads.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
            }
            for (int index = 0; index < 2000; index++) {
                if ((index % 2) == 0) {
                    Lowl::Logger::register_log_receiver(&receiver_one, &state_one);
                } else {
                    Lowl::Logger::register_log_receiver(&receiver_two, &state_two);
                }
                Lowl::Logger::set_log_level(
                    (index % 3) == 0 ? Lowl::Logger::Level::Debug : Lowl::Logger::Level::Info
                );
            }
        });

        for (int thread_index = 0; thread_index < 4; thread_index++) {
            threads.emplace_back([&]() {
                while (!start.load(std::memory_order_acquire)) {
                }
                for (int index = 0; index < 2000; index++) {
                    Lowl::Logger::write(__FILE__, __func__, __LINE__, Lowl::Logger::Level::Info, "threaded log");
                }
            });
        }

        start.store(true, std::memory_order_release);
        for (std::thread &thread : threads) {
            thread.join();
        }

        Lowl::Logger::register_log_receiver(nullptr, nullptr);
        Lowl::Logger::set_log_level(Lowl::Logger::Level::Info);

        REQUIRE_GT(state_one.calls.load(std::memory_order_relaxed) + state_two.calls.load(std::memory_order_relaxed), 0);
        REQUIRE_EQ(state_one.mismatches.load(std::memory_order_relaxed), 0);
        REQUIRE_EQ(state_two.mismatches.load(std::memory_order_relaxed), 0);
        REQUIRE_EQ(state_one.formatting_failures.load(std::memory_order_relaxed), 0);
        REQUIRE_EQ(state_two.formatting_failures.load(std::memory_order_relaxed), 0);
    }
}
