#ifdef QUEST
#include "main.hpp"
#include "mainquest.hpp"
#include <thread>
#include <android/log.h>

#include "SocketLogger.hpp"
#include "fmt/format.h"

using namespace SocketLib;

static ModInfo modInfo; // Stores the ID and version of our mod, and is sent to the modloader upon startup

// Called at the early stages of game loading
extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;

    // Subscribe to logger
    Logger::loggerCallback += [](LoggerLevel level, std::string_view tag, std::string_view const log){
        __android_log_print(ANDROID_LOG_DEBUG, fmt::format("[{}] QuestHook[{}]", Logger::loggerLevelToStr(level), tag).c_str(), "%s", log.data());
    };
}

// Called later on in the game loading - a good time to install function hooks
extern "C" void load() {
    // Run on a separate thread so the main thread doesn't freeze
    // Note this means the server will run indefinitely in the game
    std::thread([]{
          startTests(true);
    }).detach();
}
#endif