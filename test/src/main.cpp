#include "main.hpp"

#include "SocketHandler.hpp"
#include "ServerSocketTest.hpp"
#include "ClientSocketTest.hpp"

#include "SocketLogger.hpp"

#include <iostream>
#include <exception>

using namespace SocketLib;

// NON-QUEST TESTS
#ifndef QUEST
int main(int argc, char *argv[]) {
//#ifdef __GNUC__
//    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
//#endif


    try {
        std::span<char*> args(argv, argc);

        std::cout << "Starting tests" << std::endl;
        startTests(!std::any_of(args.begin(), args.end(), [](auto const& a) {return a == "client"; }));
        std::cout << "Finished tests" << std::endl;
    } catch (std::exception& e) {
        std::cout << "Test failed due to: " << e.what() << std::endl;
        return -1;
    }
}

void handleLog(LoggerLevel level, std::string_view const tag, std::string_view const log) {
    fmt::print("[{}] ({}): {}\n", Logger::loggerLevelToStr(level), tag, log);
}
#endif

void startTests(bool server) {
    // Subscribe to logger
    SocketHandler::getCommonSocketHandler().getLogger().loggerCallback += handleLog;

    if (server) {
        std::cout << "Server test " << std::endl;
        ServerSocketTest serverSocketTest{};
        serverSocketTest.startTest();
    } else {
        std::cout << "Client test " << std::endl;
        ClientSocketTest clientSocketTest{};
        clientSocketTest.startTest();
    }
}

Logger &getLogger() {
    return SocketHandler::getCommonSocketHandler().getLogger();
}
