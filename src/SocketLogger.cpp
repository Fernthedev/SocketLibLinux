#include "SocketLogger.hpp"


#include <android/log.h>

using namespace SocketLib;

inline void setup() {
    static bool setup;
    if (setup) return;

    setup = true;
    Paper::Logger::RegisterFileContextId("SocketLib");
}

void Logger::queueLogInternal(const moodycamel::ProducerToken &producer, LoggerLevel level, std::string_view tag,
                              std::string_view log) {
    setup();
#ifdef SOCKETLIB_PAPER_LOG
#warning Using paper logger
    __android_log_print(static_cast<int>(level), "SocketLib", "[%s] %s", tag.data(), log.data());
    Paper::Logger::vfmtLog("[{}] {}", static_cast<Paper::LogLevel>(level), Paper::sl::current("", "", 0, 0),
                           "SocketLib", fmt::make_format_args(tag, log));
#else
    while(!logQueue.enqueue(producer, {level, tag, log})) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
#endif
}

void Logger::queueLogInternal(LoggerLevel level, std::string_view tag, std::string_view log) {
    setup();

#ifdef SOCKETLIB_PAPER_LOG
#warning Using paper logger
    __android_log_print(static_cast<int>(level), "SocketLib", "[%s] %s", tag.data(), log.data());
    Paper::Logger::vfmtLog("[{}] {}", static_cast<Paper::LogLevel>(level), Paper::sl::current("", "", 0, 0),
                           "SocketLib", fmt::make_format_args(tag, log));
#else
    while(!logQueue.enqueue({level, tag, log})) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
#endif
}
