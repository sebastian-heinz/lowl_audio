#include "lowl_profiler.h"

#include "lowl_logger.h"

#include <sstream>

std::unique_ptr<moodycamel::ConcurrentQueue<Lowl::Profiler::Event>>Lowl::Profiler::events = std::make_unique<moodycamel::ConcurrentQueue<Lowl::Profiler::Event>>(
        10000000, 5, 5
);
std::map<std::string, Lowl::Profiler::Measure> Lowl::Profiler::measures = std::map<std::string, Lowl::Profiler::Measure>();
std::atomic_flag Lowl::Profiler::is_running = ATOMIC_FLAG_INIT;
std::thread Lowl::Profiler::thread;
Lowl::Profiler::ProfileMessageReceiver Lowl::Profiler::receiver = nullptr;
void *Lowl::Profiler::user_data = nullptr;
Lowl::uint32_l Lowl::Profiler::receive_ms = 10 * 1000;
std::chrono::high_resolution_clock::time_point Lowl::Profiler::last_receive = std::chrono::high_resolution_clock::now();

void Lowl::Profiler::stop_time(const std::string &p_name) {
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::duration<TimeNsEpoch, std::nano>> now
            = std::chrono::high_resolution_clock::now();
    std::chrono::duration<TimeNsEpoch, std::nano> now_ns_duration = now.time_since_epoch();

    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::duration<TimeNsEpoch, std::milli>> now_ms
            = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    std::chrono::duration<TimeNsEpoch, std::milli> now_ms_duration = now_ms.time_since_epoch();


    Event event = {};
    event.time = now_ns_duration.count();
    event.type = EventType::EndTime;
    event.name = p_name;
    event.thread_id = std::this_thread::get_id();
    events->enqueue(event);
}

void Lowl::Profiler::start_time(const std::string &p_name) {
    std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::duration<long long, std::nano>> now
            = std::chrono::high_resolution_clock::now();
    std::chrono::duration<long long, std::nano> now_ns_duration = now.time_since_epoch();

    Event event = {};
    event.time = now_ns_duration.count();
    event.type = EventType::StartTime;
    event.name = p_name;
    event.thread_id = std::this_thread::get_id();
    events->enqueue(event);
}

void Lowl::Profiler::profile_number(const std::string &p_name, double p_number) {
    Event event = {};
    event.type = EventType::ProfileDoubleNumber;
    event.name = p_name;
    event.thread_id = std::this_thread::get_id();
    event.double_number = p_number;
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
                        Measure measure = create_measure(event, thread_id, identifier);
                        measures.insert(std::pair<std::string, Measure>(measure.identifier, measure));
                    } else {
                        Measure &measure = it->second;
                        measure.time_start = event.time;
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
                    measure.time_end = event.time;

                    if (measure.time_start > measure.time_end) {
                        Logger::log(Logger::Level::Profiling,
                                    "Lowl::Profiler::thread_func: event time is negative (measure.start_time > measure.end_time)");
                        continue;
                    }

                    TimeNsEpoch duration_ns = measure.time_end - measure.time_start;
                    measure.time_duration_ms = duration_ns / 1000000.0;
                    measure.count++;
                    if (measure.time_max_duration_ms < measure.time_duration_ms) {
                        measure.time_max_duration_ms = measure.time_duration_ms;
                    }
                    if (measure.time_min_duration_ms > measure.time_duration_ms) {
                        measure.time_min_duration_ms = measure.time_duration_ms;
                    }
                    measure.time_total_duration_ms += measure.time_duration_ms;
                    measure.time_average_duration_ms = measure.time_total_duration_ms / measure.count;
                    break;
                }
                case EventType::ProfileDoubleNumber: {
                    if (it == measures.end()) {
                        Measure measure = create_measure(event, thread_id, identifier);
                        measures.insert(std::pair<std::string, Measure>(measure.identifier, measure));
                    } else {
                        Measure &measure = it->second;
                        measure.double_number = event.double_number;
                        measure.count++;
                        if (measure.double_max_number < measure.double_number) {
                            measure.double_max_number = measure.double_number;
                        }
                        if (measure.double_min_number > measure.double_number) {
                            measure.double_min_number = measure.double_number;
                        }
                        measure.double_total += measure.double_number;
                        measure.double_average = measure.double_total / measure.count;
                    }
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
        std::this_thread::sleep_for(std::chrono::milliseconds(receive_ms / 2));
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

void Lowl::Profiler::register_profiling_receiver(Profiler::ProfileMessageReceiver p_receiver, uint32_l p_receive_ms,
                                                 void *p_user_data) {
    receiver = p_receiver;
    user_data = p_user_data;
    if (p_receive_ms <= 0) {
        p_receive_ms = MINIMUM_RECEIVE_MS;
    }
    receive_ms = p_receive_ms;
}

void Lowl::Profiler::std_out_profiling_receiver(Measure p_measure, void *p_user_data) {
    std::string msg;
    msg += "\n----------\n";
    msg += p_measure.name + " (" + p_measure.thread_id + ")\n";
    msg += "Cnt: " + std::to_string(p_measure.count) + "\n";
    if (p_measure.type == MeasureType::Time) {
        msg += "Now: " + std::to_string(p_measure.time_duration_ms) + "ms\n";
        msg += "Min: " + std::to_string(p_measure.time_min_duration_ms) + "ms\n";
        msg += "Max: " + std::to_string(p_measure.time_max_duration_ms) + "ms\n";
        msg += "Avg: " + std::to_string(p_measure.time_average_duration_ms) + "ms\n";
        msg += "Ttl: " + std::to_string(p_measure.time_total_duration_ms) + "ms\n";
    } else if (p_measure.type == MeasureType::DoubleNumber) {
        msg += "Now: " + std::to_string(p_measure.double_number) + "\n";
        msg += "Min: " + std::to_string(p_measure.double_min_number) + "\n";
        msg += "Max: " + std::to_string(p_measure.double_max_number) + "\n";
        msg += "Avg: " + std::to_string(p_measure.double_average) + "\n";
        msg += "Ttl: " + std::to_string(p_measure.double_total) + "\n";
    }
    msg += "----------\n";
    Logger::log(Logger::Level::Profiling, msg);
}

void Lowl::Profiler::register_std_out_profiling_receiver(uint32_l p_receive_ms) {
    register_profiling_receiver(&Profiler::std_out_profiling_receiver, p_receive_ms, nullptr);
    Logger::log(Logger::Level::Profiling, "StdOut profiling registered");
}

std::string Lowl::Profiler::get_thread_id_hex(std::thread::id p_thread_id) {
    std::stringstream thread_id("0x");
    thread_id << std::hex << p_thread_id;
    return thread_id.str();
}

Lowl::Profiler::Measure
Lowl::Profiler::create_measure(const Event &p_event, std::string thread_id, std::string identifier) {
    Measure measure = Measure();

    measure.name = p_event.name;
    measure.thread_id = thread_id;
    measure.identifier = identifier;
    measure.count = 0;

    measure.time_start = 0;
    measure.time_end = 0;
    measure.time_duration_ms = 0;
    measure.time_min_duration_ms = std::numeric_limits<double>::max();
    measure.time_max_duration_ms = std::numeric_limits<double>::min();
    measure.time_average_duration_ms = 0;
    measure.time_total_duration_ms = 0;

    measure.double_number = 0;
    measure.double_total = 0;
    measure.double_average = 0;
    measure.double_max_number = std::numeric_limits<double>::min();
    measure.double_min_number = std::numeric_limits<double>::max();

    switch (p_event.type) {
        case EventType::ProfileDoubleNumber: {
            measure.type = MeasureType::DoubleNumber;
            measure.double_number = p_event.double_number;
            break;
        }
        case EventType::StartTime:
            measure.type = MeasureType::Time;
            measure.time_start = p_event.time;
            break;
        case EventType::EndTime:
            measure.type = MeasureType::Time;
            measure.time_end = p_event.time;
            break;
    }

    return measure;
}
