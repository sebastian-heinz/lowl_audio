#ifndef LOWL_PROFILER_H
#define LOWL_PROFILER_H

#include "lowl_typedef.h"

#include <concurrentqueue.h>

#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <map>

namespace Lowl {
    class Profiler {
    public:
        static constexpr Lowl::uint32_l MINIMUM_RECEIVE_MS = 2;

        enum class MeasureType {
            Time = 1,
            DoubleNumber = 2,
        };

        struct Measure {
            MeasureType type;
            std::string identifier;
            std::string name;
            std::string thread_id;
            size_t count;
            union {
                struct {
                    TimeNsEpoch time_start;
                    TimeNsEpoch time_end;
                    double time_duration_ms;
                    double time_max_duration_ms;
                    double time_min_duration_ms;
                    double time_total_duration_ms;
                    double time_average_duration_ms;
                };
                struct {
                    double double_number;
                    double double_max_number;
                    double double_min_number;
                    double double_total;
                    double double_average;
                };
            };
        };

        typedef void (*ProfileMessageReceiver)(Measure p_measure, void *p_user_data);

    private:
        enum class EventType {
            StartTime = 1,
            EndTime = 2,
            ProfileDoubleNumber = 3,
        };

        struct Event {
            std::string name;
            std::thread::id thread_id;
            EventType type;
            union {
                TimeNsEpoch time;
                double double_number;
            };
        };

        static ProfileMessageReceiver receiver;
        static void *user_data;
        static std::map<std::string, Measure> measures;
        static std::unique_ptr<moodycamel::ConcurrentQueue<Event>> events;
        static std::thread thread;
        static std::atomic_flag is_running;
        static uint32_l receive_ms;
        static std::chrono::high_resolution_clock::time_point last_receive;

        Profiler() {
            // Disallow creating an instance of this object
        };

        static std::string get_thread_id_hex(std::thread::id p_thread_id);

        static void thread_func();

        static void std_out_profiling_receiver(Measure p_measure, void *p_user_data);

        static Measure create_measure(const Event &p_event, std::string thread_id, std::string identifier);

    public:
        static void
        register_profiling_receiver(ProfileMessageReceiver p_receiver, uint32_l p_receive_ms, void *p_user_data);

        static void register_std_out_profiling_receiver(uint32_l p_receive_ms);

        static void start();

        static void stop();

        static void start_time(const std::string &p_name);

        static void stop_time(const std::string &p_name);

        static void profile_number(const std::string &p_name, double p_number);
    };
}

#ifdef LOWL_PROFILING
#define LP_START_TIME(name) Lowl::Profiler::start_time(name)
#define LP_STOP_TIME(name) Lowl::Profiler::stop_time(name)
#define LP_NUMBER(name, number) Lowl::Profiler::profile_number(name, number)
#else
#define LP_START_TIME(name)
#define LP_STOP_TIME(name)
#define LP_NUMBER(name, number)
#endif

#endif // LOWL_PROFILER_H
