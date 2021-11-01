#ifndef LOWL_LOGGER_H
#define LOWL_LOGGER_H

#include <string>
#include <chrono>
#include <ctime>

namespace Lowl {

    class Logger {
    public:
        enum class Level {
            Error = 50,
            Warn = 40,
            Info = 30,
            Debug = 10,
        };

        struct Log {
            std::string file_name;
            int line;
            std::string function_name;
            Level level;
            std::string message;
        };

        typedef void (*LogMessageReceiver)(const Log &p_log, void *p_user_data);

    private:
        static LogMessageReceiver receiver;
        static void *user_data;
        static Level log_level;

        Logger() {
            // Disallow creating an instance of this object
        };

        template<typename T>
        static int to_ms(const std::chrono::time_point<T> &tp);

        static std::string pretty_time();

        static void std_out_log_receiver(const Log &p_log, void *p_user_data);

    public:
        static void set_log_level(Level p_level);

        static void register_log_receiver(LogMessageReceiver p_receiver, void *p_user_data);

        static void register_std_out_log_receiver();

        static void write(const Log &p_log);

        static void write(const char *p_file_name, const char *p_file_function, int p_file_line, Level p_level,
                          std::string p_message);

        static std::string format_log(const Log &p_log);

        static std::string format_level(Level p_level);

        static std::string format_arguments(const char *const p_fmt, ...)
        __attribute__ ((format (printf, 1, 2)));
    };

}

#ifdef LOWL_DEBUG
#define LOWL_LOG_F(level, fmt, ...) Lowl::Logger::write(__FILE__, __FUNCTION__, __LINE__, level, Lowl::Logger::format_arguments(fmt, __VA_ARGS__))
#define LOWL_LOG(level, fmt) Lowl::Logger::write(__FILE__, __FUNCTION__, __LINE__, level, fmt)
#else
#define LOWL_LOG_F(level, fmt, ...) (void)0
#define LOWL_LOG(level, fmt) (void)0
#endif

#define LOWL_LOG_DEBUG_F(fmt, ...) LOWL_LOG_F(Lowl::Logger::Level::Debug, fmt, __VA_ARGS__)
#define LOWL_LOG_DEBUG(fmt) LOWL_LOG(Lowl::Logger::Level::Debug, fmt)

#define LOWL_LOG_INFO_F(fmt, ...) LOWL_LOG_F(Lowl::Logger::Level::Info, fmt, __VA_ARGS__)
#define LOWL_LOG_INFO(fmt) LOWL_LOG(Lowl::Logger::Level::Info, fmt)

#define LOWL_LOG_WARN_F(fmt, ...) LOWL_LOG_F(Lowl::Logger::Level::Warn, fmt, __VA_ARGS__)
#define LOWL_LOG_WARN(fmt) LOWL_LOG(Lowl::Logger::Level::Warn, fmt)

#define LOWL_LOG_ERROR_F(fmt, ...) LOWL_LOG_F(Lowl::Logger::Level::Error, fmt, __VA_ARGS__)
#define LOWL_LOG_ERROR(fmt) LOWL_LOG(Lowl::Logger::Level::Error, fmt)

#endif