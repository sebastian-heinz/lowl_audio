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
        static void register_log_receiver(LogMessageReceiver p_receiver, void *p_user_data);

        static void log(Level p_level, std::string p_message);

        static void register_std_out_log_receiver();

        static void set_log_level(Level p_level);
    };
}

#endif