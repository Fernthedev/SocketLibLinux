#pragma once

#include "utils/EventCallback.hpp"
#include "queue/blockingconcurrentqueue.h"

#include <string>
#include <string_view>
#include <utility>
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
        Logger() = default;
        Logger(Logger const&) = delete;

        static constexpr std::string_view loggerLevelToStr(LoggerLevel level) {
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

        bool DebugEnabled;
        Utils::UnorderedEventCallback<LoggerLevel, std::string const&, std::string const&> loggerCallback;

        template<LoggerLevel lvl, typename... TArgs>
        constexpr void fmtLog(std::string_view tag, fmt::format_string<TArgs...> str, TArgs&&... args) {
            writeLog<lvl>(tag, fmt::format<TArgs...>(str, std::forward<TArgs>(args)...));
        }

        template<typename Exception = std::runtime_error, typename... TArgs>
        inline void fmtThrowError(std::string_view tag, fmt::format_string<TArgs...> str, TArgs&&... args) {
            fmtLog<LoggerLevel::ERROR, TArgs...>(tag, str, std::forward<TArgs>(args)...);
            throw Exception(fmt::format<TArgs...>(str, std::forward<TArgs>(args)...));
        }

        template<LoggerLevel lvl>
        constexpr void writeLog(std::string_view const tag, std::string_view const log) {
            if constexpr (lvl == LoggerLevel::DEBUG_LEVEL) {
                if (!DebugEnabled) return;
            }
            queueLogInternal(lvl, tag, log);
        }

        void queueLogInternal(LoggerLevel level, std::string_view tag, std::string_view log) {
            logQueue.enqueue({level, tag, log});
        }

        [[nodiscard]] moodycamel::ProducerToken createProducerToken() {
            return moodycamel::ProducerToken(logQueue);
        }

        // producer stuff
#pragma region producer
        template<LoggerLevel lvl, typename... TArgs>
        constexpr void fmtLog(moodycamel::ProducerToken& producer, std::string_view tag, fmt::format_string<TArgs...> str, TArgs&&... args) {
            writeLog<lvl>(producer, tag, fmt::format<TArgs...>(str, std::forward<TArgs>(args)...));
        }

        template<typename Exception = std::runtime_error, typename... TArgs>
        inline void fmtThrowError(moodycamel::ProducerToken& producer, std::string_view tag, fmt::format_string<TArgs...> str, TArgs&&... args) {
            fmtLog<LoggerLevel::ERROR, TArgs...>(producer, tag, str, std::forward<TArgs>(args)...);
            throw Exception(fmt::format<TArgs...>(str, std::forward<TArgs>(args)...));
        }

        template<LoggerLevel lvl>
        constexpr void writeLog(moodycamel::ProducerToken& token, std::string_view const tag, std::string_view const log) {
            if constexpr (lvl == LoggerLevel::DEBUG_LEVEL) {
                if (!DebugEnabled) return;
            }
            queueLogInternal(token, lvl, tag, log);
        }

        void queueLogInternal(moodycamel::ProducerToken& producer, LoggerLevel level, std::string_view tag, std::string_view log) {
            while(!logQueue.enqueue(producer, {level, tag, log}));
        }
#pragma endregion

    private:
        struct LogTask {
            LoggerLevel level = LoggerLevel::DEBUG_LEVEL;
            std::string tag;
            std::string log;

            LogTask() = default;
            LogTask(LoggerLevel level, std::string_view tag, std::string_view log) : level(level), tag(tag),
                                                                                         log(log) {}
        };

        moodycamel::BlockingConcurrentQueue<LogTask> logQueue;

        friend class SocketHandler;
    };
}