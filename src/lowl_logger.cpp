#include "lowl_logger.h"

#include <iostream>
#include <vector>
#include <cstdarg>

#define LOGGER_PRETTY_TIME_FORMAT "%Y-%m-%d %H:%M:%S"
#define LOGGER_PRETTY_MS_FORMAT ".%03d"
#define LOGGER_FORMAT "[%s][%s]%s: %s ([%s] @ %s:%d)"
#define LOGGER_PREFIX "LOWL"

namespace Lowl {

    Logger::LogMessageReceiver Logger::receiver = nullptr;
    void *Logger::user_data = nullptr;
    Logger::Level Logger::log_level = Level::Info;

    void Logger::set_log_level(Logger::Level p_level) {
        log_level = p_level;
    }

    void Logger::register_log_receiver(Logger::LogMessageReceiver p_receiver, void *p_user_data) {
        receiver = p_receiver;
        user_data = p_user_data;
    }

    void Logger::register_std_out_log_receiver() {
        register_log_receiver(&Logger::std_out_log_receiver, nullptr);
        LOWL_LOG(Logger::Level::Debug, "StdOut logger registered");
    }

    void Logger::std_out_log_receiver(const Log &p_log, void *p_user_data) {
        if (p_log.level < log_level) {
            return;
        }
        std::string log = format_log(p_log);
        std::cout << log << '\n';
    }

    void Logger::write(const Log &p_log) {
        if (!receiver) {
            return;
        }
        receiver(p_log, user_data);
    }

    void Logger::write(const char *p_file_name,
                       const char *p_file_function,
                       int p_file_line,
                       Level p_level,
                       const std::string &p_message) {
        Log log{
                std::string(p_file_name),
                p_file_line,
                std::string(p_file_function),
                p_level,
                p_message
        };
        write(log);
    }

    template<typename T>
    int Logger::to_ms(const std::chrono::time_point<T> &tp) {
        auto dur = tp.time_since_epoch();
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(dur).count());
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

    std::string Logger::format_arguments(const char *const p_fmt, ...) {
        std::vector<char> temp = std::vector<char>{};
        std::size_t length = 63;
        std::va_list args;
        while (temp.size() <= length) {
            temp.resize(length + 1);
            va_start(args, p_fmt);
            const int status = std::vsnprintf(temp.data(), temp.size(), p_fmt, args);
            va_end(args);
            if (status < 0) {
                throw std::runtime_error{"string formatting error"};
            };
            length = static_cast<std::size_t>(status);
        }
        return std::string{temp.data(), length};
    }

    std::string Logger::format_level(Logger::Level p_level) {
        switch (p_level) {
            case Level::Error:
                return "[Error]";
            case Level::Warn:
                return "[Warn ]";
            case Level::Info:
                return "[Info ]";
            case Level::Debug:
                return "[Debug]";
        }
        return "";
    }

    std::string Logger::format_log(const Logger::Log &p_log) {
        std::string now = pretty_time();
        std::string level = format_level(p_log.level);
        int size = std::snprintf(
                nullptr,
                0,
                LOGGER_FORMAT,
                now.c_str(),
                LOGGER_PREFIX,
                level.c_str(),
                p_log.function_name.c_str(),
                p_log.message.c_str(),
                p_log.file_name.c_str(),
                p_log.line
        );
        std::vector<char> buf(static_cast<size_t>(size + 1)); // note +1 for null terminator
        std::snprintf(
                &buf[0],
                buf.size(),
                LOGGER_FORMAT,
                now.c_str(),
                LOGGER_PREFIX,
                level.c_str(),
                p_log.message.c_str(),
                p_log.function_name.c_str(),
                p_log.file_name.c_str(),
                p_log.line
        );
        return std::string{buf.data(), buf.size()};
    }


}