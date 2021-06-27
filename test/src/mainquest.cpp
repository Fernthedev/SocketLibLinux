#ifdef QUEST
#include "main.hpp"
#include "mainquest.hpp"
#include <thread>

static ModInfo modInfo; // Stores the ID and version of our mod, and is sent to the modloader upon startup

// Called at the early stages of game loading
extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
}

// Called later on in the game loading - a good time to install function hooks
extern "C" void load() {
    // Run on a separate thread so the main thread doesn't freeze
    // Note this means the server will run indefinitely in the game
    std::thread([]{
          startTests();
    }).detach();
}
#endif