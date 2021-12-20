#include "SocketLogger.hpp"

using namespace SocketLib;

Utils::UnorderedEventCallback<LoggerLevel, std::string_view, std::string const &> Logger::loggerCallback;

void SocketLib::Logger::writeLog(SocketLib::LoggerLevel level, std::string_view tag, const std::string &log) {
    loggerCallback.invoke(level, tag, log);
}

