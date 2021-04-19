#include <string>
#include <chrono>
#include <ctime>

namespace Lowl {

    class Logger {
    public:
        enum class Level {
            Error = 40,
            Warn = 30,
            Info = 20,
            Debug = 10,
        };

        typedef void (*LogMessageReceiver)(Level p_level, char const *p_message, void *p_user_data);


    private:
        static LogMessageReceiver receiver;
        static void *user_data;

        template<typename T>
        static int to_ms(const std::chrono::time_point<T> &tp);

        static std::string pretty_time();

        static void std_out_log_receiver(Level p_level, char const *p_message, void *p_user_data);

    public:
        static void register_log_receiver(LogMessageReceiver p_receiver, void *p_user_data);

        static void log(Level p_level, std::string p_message);

        static void register_std_out_log_receiver();

        Logger() = default;

        ~Logger() = default;
    };
}

//#endif