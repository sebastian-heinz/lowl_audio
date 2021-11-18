#ifndef LOWL_TIMER_H
#define LOWL_TIMER_H

#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

namespace Lowl {

    class Timer {

    private:
        std::atomic_flag running = ATOMIC_FLAG_INIT;
        std::thread *thread;
        std::chrono::duration<double, std::milli> duration;
        std::function<void()> thread_function;

        void thread_interval() {
            running.test_and_set();
            while (running.test_and_set()) {
                thread_function();
                std::this_thread::sleep_for(duration);
            }
        }

        void thread_timer() {
            std::this_thread::sleep_for(duration);
            thread_function();
        }

    public:
        template<typename Rep, typename Period>
        void start_interval(std::function<void()> p_thread_function, std::chrono::duration<Rep, Period> p_interval) {
            stop();
            thread_function = p_thread_function;
            duration = p_interval;
            thread = new std::thread(&Timer::thread_interval, this);
        }

        template<typename Rep, typename Period>
        void start_timer(std::function<void()> p_thread_function, std::chrono::duration<Rep, Period> p_duration) {
            stop();
            thread_function = p_thread_function;
            duration = p_duration;
            thread = new std::thread(&Timer::thread_timer, this);
        }

        void stop() {
            if (thread != nullptr) {
                running.clear();
                thread->join();
                delete thread;
                thread = nullptr;
                thread_function = nullptr;
            }
        }

        Timer() {
            thread = nullptr;
            running.clear();
            duration = std::chrono::seconds(0);
            thread_function = nullptr;
        }

        ~Timer() {
            stop();
        }
    };
}

#endif