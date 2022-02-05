#pragma once

#include "utils/EventCallback.hpp"

#include <string>
#include <string_view>
#include <fmt/core.h>

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
                default:
                    return "UNKNOWN LEVEL";
            }
        }

        static bool DebugEnabled;

        static Utils::UnorderedEventCallback<LoggerLevel, std::string_view const, std::string_view const> loggerCallback;

        template<LoggerLevel lvl, typename... TArgs>
        constexpr static void fmtLog(std::string_view tag, fmt::format_string<TArgs...> str, TArgs&&... args) {
            writeLog<lvl>(tag, fmt::format<TArgs...>(str, std::forward<TArgs>(args)...));
        }

        template<typename Exception = std::runtime_error, typename... TArgs>
        inline static void fmtThrowError(std::string_view tag, fmt::format_string<TArgs...> str, TArgs&&... args) {
            fmtLog<LoggerLevel::ERROR, TArgs...>(tag, str, std::forward<TArgs>(args)...);
            throw Exception(fmt::format<TArgs...>(str, std::forward<TArgs>(args)...));
        }

        template<LoggerLevel lvl>
        constexpr static void writeLog(std::string_view const tag, std::string_view const log) {
            if constexpr (lvl == LoggerLevel::DEBUG_LEVEL) {
                if (!DebugEnabled) return;
            }
            writeLogInternal(lvl, tag, log);
        }

        static void writeLogInternal(LoggerLevel level, std::string_view tag, std::string_view log);
    };
}