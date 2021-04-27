#ifndef LOWL_PROFILER_H
#define LOWL_PROFILER_H

#include <concurrentqueue.h>

#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <map>

namespace Lowl {
    class Profiler {
    public:
        struct Measure {
            std::string identifier;
            std::string name;
            std::string thread_id;
            std::chrono::high_resolution_clock::time_point start_time;
            std::chrono::high_resolution_clock::time_point end_time;
            size_t count;
            double duration_ms;
            double max_duration_ms;
            double min_duration_ms;
            double total_duration_ms;
            double average_duration_ms;
        };

        typedef void (*ProfileMessageReceiver)(Measure p_measure, void *p_user_data);

    private:
        enum class EventType {
            StartTime = 1,
            EndTime = 2,
        };

        struct Event {
            std::string name;
            std::thread::id thread_id;
            EventType type;
            std::chrono::high_resolution_clock::time_point time;
        };

        static ProfileMessageReceiver receiver;
        static void *user_data;
        static std::map<std::string, Measure> measures;
        static std::unique_ptr<moodycamel::ConcurrentQueue<Event>> events;
        static std::thread thread;
        static std::atomic_flag is_running;
        static double receive_ms;
        static std::chrono::high_resolution_clock::time_point last_receive;

        Profiler() {
            // Disallow creating an instance of this object
        };

        static std::string get_thread_id_hex(std::thread::id p_thread_id);

        static void thread_func();

        static void std_out_profiling_receiver(Measure p_measure, void *p_user_data);

    public:
        static void
        register_profiling_receiver(ProfileMessageReceiver p_receiver, double p_receive_ms, void *p_user_data);

        static void register_std_out_profiling_receiver(double p_receive_ms);

        static void start();

        static void stop();

        static void start_time(const std::string &p_name);

        static void stop_time(const std::string &p_name);
    };
}

#ifdef LOWL_PROFILING
#define LP_START_TIME(name) Lowl::Profiler::start_time(name)
#define LP_STOP_TIME(name) Lowl::Profiler::stop_time(name)
#else
#define LP_START_TIME(name)
#define LP_STOP_TIME(name)
#endif

#endif // LOWL_PROFILER_H
