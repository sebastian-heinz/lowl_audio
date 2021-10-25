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
            Profiling = 20,
            Debug = 10,
        };

        typedef void (*LogMessageReceiver)(Level p_level, char const *p_message, void *p_user_data);

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

        static void std_out_log_receiver(Level p_level, char const *p_message, void *p_user_data);

    public:
        static std::string format(const char *const p_fmt, ...)
        __attribute__ ((format (printf, 1, 2)));

        static void register_log_receiver(LogMessageReceiver p_receiver, void *p_user_data);

        static void log(Level p_level, std::string p_message);

        static void log(const char *p_file_name, const char *p_file_function, int p_file_line, Level p_level, std::string p_message);

        static void register_std_out_log_receiver();

        static void set_log_level(Level p_level);
    };

}

#ifdef LOWL_DEBUG
#define LOWL_LOG_F(level, fmt, ...) Lowl::Logger::log(__FILE__, __FUNCTION__, __LINE__, level, Lowl::Logger::format(fmt, __VA_ARGS__))
#define LOWL_LOG(level, fmt) Lowl::Logger::log(__FILE__, __FUNCTION__, __LINE__, level, fmt)
#else
#define LOWL_LOG_F(level, fmt, ...) (void)0
#define LOWL_LOG(level, fmt) (void)0
#endif

#define LOWL_LOG_INFO_F(fmt, ...) LOWL_LOG_F(Lowl::Logger::Level::Info, fmt, __VA_ARGS__)
#define LOWL_LOG_INFO(fmt) LOWL_LOG(Lowl::Logger::Level::Info, fmt)

#define LOWL_LOG_ERROR_F(fmt, ...) LOWL_LOG_F(Lowl::Logger::Level::Error, fmt, __VA_ARGS__)
#define LOWL_LOG_ERROR(fmt) LOWL_LOG(Lowl::Logger::Level::Error, fmt)

#endif