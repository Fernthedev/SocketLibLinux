#include "SocketLogger.hpp"

using namespace SocketLib;

bool Logger::DebugEnabled = true;
Utils::UnorderedEventCallback<LoggerLevel, std::string_view const, std::string_view const> Logger::loggerCallback;

void SocketLib::Logger::writeLogInternal(SocketLib::LoggerLevel level, std::string_view const tag, std::string_view const log) {
    loggerCallback.invoke(level, tag, log);
}

