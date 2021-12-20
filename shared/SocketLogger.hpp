#pragma once

#include "utils/EventCallback.hpp"

#include <string>
#include <string_view>

namespace SocketLib {

    enum class LoggerLevel {
        DEBUG_LEVEL,
        INFO,
        WARN,
        ERROR
    };

    class Logger {
    public:
        constexpr static std::string_view loggerLevelToStr(LoggerLevel level) {
            switch (level) {
                case LoggerLevel::DEBUG_LEVEL:
                    return "DEBUG";
                case LoggerLevel::INFO:
                    return "INFO";
                case LoggerLevel::WARN:
                    return "WARN";
                case LoggerLevel::ERROR:
                    return "ERROR";
            }
        }

        static Utils::UnorderedEventCallback<LoggerLevel, std::string_view, std::string const &> loggerCallback;

        static void writeLog(LoggerLevel level, std::string_view tag, std::string const &log);
    };
}