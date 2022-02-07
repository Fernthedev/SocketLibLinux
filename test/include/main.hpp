#pragma once

#include "SocketLogger.hpp"

void startTests(bool server);

void handleLog(SocketLib::LoggerLevel level, std::string_view tag, std::string_view log);

[[nodiscard]] SocketLib::Logger& getLogger();