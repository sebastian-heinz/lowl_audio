#include "lowl_profiler.h"

#include "lowl_logger.h"

#include <sstream>

std::unique_ptr<moodycamel::ConcurrentQueue<Lowl::Profiler::Event>>Lowl::Profiler::events = std::make_unique<moodycamel::ConcurrentQueue<Lowl::Profiler::Event>>();
std::map<std::string, Lowl::Profiler::Measure> Lowl::Profiler::measures = std::map<std::string, Lowl::Profiler::Measure>();
std::atomic_flag Lowl::Profiler::is_running = ATOMIC_FLAG_INIT;
std::thread Lowl::Profiler::thread;
Lowl::Profiler::ProfileMessageReceiver Lowl::Profiler::receiver = nullptr;
void *Lowl::Profiler::user_data = nullptr;
double Lowl::Profiler::receive_ms = 10 * 1000;
std::chrono::high_resolution_clock::time_point Lowl::Profiler::last_receive = std::chrono::high_resolution_clock::now();

void Lowl::Profiler::stop_time(const std::string &p_name) {
    Event event = {};
    event.time = std::chrono::high_resolution_clock::now();
    event.type = EventType::EndTime;
    event.name = p_name;
    event.thread_id = std::this_thread::get_id();
    events->enqueue(event);
}

void Lowl::Profiler::start_time(const std::string &p_name) {
    Event event = {};
    event.type = EventType::StartTime;
    event.name = p_name;
    event.thread_id = std::this_thread::get_id();
    event.time = std::chrono::high_resolution_clock::now();
    events->enqueue(event);
}

void Lowl::Profiler::thread_func() {
    while (is_running.test_and_set()) {
        Event event;
        while (events->try_dequeue(event)) {
            std::string thread_id = get_thread_id_hex(event.thread_id);
            std::string identifier = event.name + thread_id;
            std::map<std::string, Measure>::iterator it = measures.find(identifier);
            switch (event.type) {
                case EventType::StartTime: {
                    if (it == measures.end()) {
                        Measure measure = {};
                        measure.name = event.name;
                        measure.thread_id = thread_id;
                        measure.identifier = identifier;
                        measure.start_time = event.time;
                        measure.end_time = std::chrono::high_resolution_clock::time_point();
                        measure.duration_ms = 0;
                        measure.min_duration_ms = std::numeric_limits<double>::max();
                        measure.max_duration_ms = std::numeric_limits<double>::min();
                        measure.average_duration_ms = 0;
                        measure.total_duration_ms = 0;
                        measure.count = 0;
                        measures.insert(std::pair<std::string, Measure>(measure.identifier, measure));
                    } else {
                        Measure &measure = it->second;
                        measure.start_time = event.time;
                    }
                    break;
                }
                case EventType::EndTime: {
                    if (it == measures.end()) {
                        Logger::log(Logger::Level::Profiling,
                                    "Lowl::Profiler::thread_func: event not started (it == measures.end())");
                        continue;
                    }

                    Measure &measure = it->second;
                    measure.end_time = event.time;

                    if (measure.start_time > measure.end_time) {
                        Logger::log(Logger::Level::Profiling,
                                    "Lowl::Profiler::thread_func: event time is negative (measure.start_time > measure.end_time)");
                        continue;
                    }

                    std::chrono::duration<double, std::milli> ms_double = measure.end_time - measure.start_time;
                    measure.duration_ms = ms_double.count();
                    measure.count++;
                    if (measure.max_duration_ms < measure.duration_ms) {
                        measure.max_duration_ms = measure.duration_ms;
                    }
                    if (measure.min_duration_ms > measure.duration_ms) {
                        measure.min_duration_ms = measure.duration_ms;
                    }
                    measure.total_duration_ms += measure.duration_ms;
                    measure.average_duration_ms = measure.total_duration_ms / measure.count;
                    break;
                }
            }
        }
        if (receiver) {
            std::chrono::duration<double, std::milli> ms_double =
                    std::chrono::high_resolution_clock::now() - last_receive;
            if (ms_double.count() > receive_ms) {
                for (const auto &measure : measures) {
                    receiver(measure.second, user_data);
                }
                last_receive = std::chrono::high_resolution_clock::now();
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

}

void Lowl::Profiler::start() {
    is_running.test_and_set();
    measures.clear();
    thread = std::thread(&Lowl::Profiler::thread_func);
}

void Lowl::Profiler::stop() {
    is_running.clear();
    thread.join();
}

void Lowl::Profiler::register_profiling_receiver(Profiler::ProfileMessageReceiver p_receiver, double p_receive_ms,
                                                 void *p_user_data) {
    receiver = p_receiver;
    user_data = p_user_data;
    receive_ms = p_receive_ms;
}

void Lowl::Profiler::std_out_profiling_receiver(Measure p_measure, void *p_user_data) {
    std::string msg;
    msg += "\n----------\n";
    msg += p_measure.name + " (" + p_measure.thread_id + ")\n";
    msg += "Cnt: " + std::to_string(p_measure.count) + "\n";
    msg += "Min: " + std::to_string(p_measure.min_duration_ms) + "ms\n";
    msg += "Max: " + std::to_string(p_measure.max_duration_ms) + "ms\n";
    msg += "Avg: " + std::to_string(p_measure.average_duration_ms) + "ms\n";
    msg += "Ttl: " + std::to_string(p_measure.total_duration_ms) + "ms\n";
    msg += "----------\n";
    Logger::log(Logger::Level::Profiling, msg);
}

void Lowl::Profiler::register_std_out_profiling_receiver(double p_receive_ms) {
    register_profiling_receiver(&Profiler::std_out_profiling_receiver, p_receive_ms, nullptr);
    Logger::log(Logger::Level::Profiling, "StdOut profiling registered");
}

std::string Lowl::Profiler::get_thread_id_hex(std::thread::id p_thread_id) {
    std::stringstream thread_id("0x");
    thread_id << std::hex << p_thread_id;
    return thread_id.str();
}
