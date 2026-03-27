#ifndef LOWL_TIMER_H
#define LOWL_TIMER_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

namespace Lowl {

    class Timer {

    private:
        std::atomic<bool> running{false};
        std::unique_ptr<std::thread> thread;
        std::chrono::duration<double, std::milli> duration;
        std::function<void()> thread_function;

        void thread_interval() {
            while (running.load(std::memory_order_acquire)) {
                if (thread_function) {
                    thread_function();
                }
                if (!running.load(std::memory_order_acquire)) {
                    break;
                }
                std::this_thread::sleep_for(duration);
            }
        }

        void thread_timer() {
            std::this_thread::sleep_for(duration);
            if (running.load(std::memory_order_acquire) && thread_function) {
                thread_function();
            }
            running.store(false, std::memory_order_release);
        }

    public:
        template <typename Rep, typename Period>
        void start_interval(std::function<void()> p_thread_function, std::chrono::duration<Rep, Period> p_interval) {
            stop();
            thread_function = p_thread_function;
            duration = p_interval;
            running.store(true, std::memory_order_release);
            thread = std::make_unique<std::thread>(&Timer::thread_interval, this);
        }

        template <typename Rep, typename Period>
        void start_timer(std::function<void()> p_thread_function, std::chrono::duration<Rep, Period> p_duration) {
            stop();
            thread_function = p_thread_function;
            duration = p_duration;
            running.store(true, std::memory_order_release);
            thread = std::make_unique<std::thread>(&Timer::thread_timer, this);
        }

        void stop() {
            running.store(false, std::memory_order_release);
            if (!thread) {
                thread_function = nullptr;
                return;
            }
            if (thread->get_id() == std::this_thread::get_id()) {
                return;
            }
            if (thread->joinable()) {
                thread->join();
            }
            thread.reset();
            thread_function = nullptr;
        }

        Timer() {
            duration = std::chrono::seconds(0);
            thread_function = nullptr;
        }

        ~Timer() {
            stop();
        }
    };
} // namespace Lowl

#endif
