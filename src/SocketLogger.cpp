#include "SocketLogger.hpp"

using namespace SocketLib;
#ifdef SOCKETLIB_PAPER_LOG

inline void setup() {
  static bool setup;
  if (setup) {
    return;
  }

  setup = true;
  Paper::Logger::RegisterFileContextId("SocketLib");
}

#endif

void Logger::queueLogInternal(const moodycamel::ProducerToken &producer,
                              LoggerLevel level, std::string_view tag,
                              std::string_view log) {
#ifdef SOCKETLIB_PAPER_LOG
  setup();
#warning Using paper logger
  Paper::Logger::vfmtLog<Paper::LogLevel::INF>(
      "[{}] {}", Paper::sl::current("", "", 0, 0), "SocketLib",
      fmt::make_format_args(tag, log));
#else
  while (!this->logQueue.enqueue(producer, {level, tag, log})) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#endif
}

void Logger::queueLogInternal(LoggerLevel level, std::string_view tag,
                              std::string_view log) {

#ifdef SOCKETLIB_PAPER_LOG
#warning Using paper logger
  setup();
  Paper::Logger::vfmtLog<Paper::LogLevel::INF>(
      "[{}] {}", Paper::sl::current("", "", 0, 0), "SocketLib",
      fmt::make_format_args(tag, log));
#else
  while (!logQueue.enqueue({level, tag, log})) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#endif
}
