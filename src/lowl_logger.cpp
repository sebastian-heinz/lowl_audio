#include "lowl_logger.h"

#include <iostream>

#define LOGGER_PRETTY_TIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define LOGGER_PRETTY_MS_FORMAT ".%03d"
#define LOGGER_PREFIX "LOWL"

namespace Lowl {

    Lowl::Logger::LogMessageReceiver Lowl::Logger::receiver = nullptr;
    void *Lowl::Logger::user_data = nullptr;
    Lowl::Logger::Level Lowl::Logger::log_level = Level::Info;

    void Logger::log(Logger::Level p_level, std::string message) {
        if (!receiver) {
            return;
        }
        receiver(p_level, message.c_str(), user_data);
    }

    void Logger::register_log_receiver(Logger::LogMessageReceiver p_receiver, void *p_user_data) {
        receiver = p_receiver;
        user_data = p_user_data;
    }

    void Logger::register_std_out_log_receiver() {
        register_log_receiver(&Logger::std_out_log_receiver, nullptr);
        Logger::log(Logger::Level::Debug, "StdOut logger registered");
    }

    template<typename T>
    int Logger::to_ms(const std::chrono::time_point<T> &tp) {
        using namespace std::chrono;

        auto dur = tp.time_since_epoch();
        return static_cast<int>(duration_cast<milliseconds>(dur).count());
    }

    std::string Logger::pretty_time() {
        auto tp = std::chrono::system_clock::now();
        std::time_t current_time = std::chrono::system_clock::to_time_t(tp);

        // this function use static global pointer. so it is not thread safe solution
        std::tm *time_info = std::localtime(&current_time);

        char buffer[128];

        size_t string_size = strftime(
                buffer, sizeof(buffer),
                LOGGER_PRETTY_TIME_FORMAT,
                time_info
        );

        int ms = to_ms(tp) % 1000;

        string_size += (size_t) std::snprintf(
                buffer + string_size, sizeof(buffer) - string_size,
                LOGGER_PRETTY_MS_FORMAT, ms
        );

        return std::string(buffer, buffer + string_size);
    }

    void Logger::std_out_log_receiver(Logger::Level p_level, const char *p_message, void *p_user_data) {
        if (p_level < log_level) {
            return;
        }
        std::cout << '[' << pretty_time() << ']';
        std::cout << '[' << LOGGER_PREFIX << ']';
        switch (p_level) {
            case Level::Error:
                std::cout << "[Error]";
                break;
            case Level::Warn:
                std::cout << "[Warn ]";
                break;
            case Level::Info:
                std::cout << "[Info ]";
                break;
            case Level::Debug:
                std::cout << "[Debug]";
                break;
            case Level::Profiling:
                std::cout << "[Prof ]";
                break;
        }
        std::cout << ": " << p_message;
        std::cout << '\n';
    }

    void Logger::set_log_level(Logger::Level p_level) {
        log_level = p_level;
    }

}