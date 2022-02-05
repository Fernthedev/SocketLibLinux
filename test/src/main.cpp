#include "main.hpp"
#include "ServerSocketTest.hpp"
#include "ClientSocketTest.hpp"

#include "SocketLogger.hpp"

#include <iostream>
#include <exception>

using namespace SocketLib;

int main(int argc, char *argv[]) {
//#ifdef __GNUC__
//    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
//#endif


    try {
        std::span<char*> args(argv, argc);

        // Subscribe to logger
        Logger::loggerCallback += [](LoggerLevel level, std::string_view const tag, std::string_view const log){
            fmt::print("[{}] ({}): {}\n", Logger::loggerLevelToStr(level), tag, log);
        };

        std::cout << "Starting tests" << std::endl;
        startTests(std::any_of(args.begin(), args.end(), [](auto const& a) {return a == "client"; }));
        std::cout << "Finished tests" << std::endl;
    } catch (std::exception& e) {
        std::cout << "Test failed due to: " << e.what() << std::endl;
        return -1;
    }
}

void startTests(bool server) {
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