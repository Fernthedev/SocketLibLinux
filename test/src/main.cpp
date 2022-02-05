#include "main.hpp"
#include "ServerSocketTest.hpp"

#include "SocketLogger.hpp"

#include <iostream>
#include <exception>

using namespace SocketLib;

int main() {
//#ifdef __GNUC__
//    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
//#endif


    try {
        // Subscribe to logger
        Logger::loggerCallback += [](LoggerLevel level, std::string_view const tag, std::string_view const log){
            fmt::print("[{}] ({}): {}\n", Logger::loggerLevelToStr(level), tag, log);
        };

        std::cout << "Starting tests" << std::endl;
        startTests();
        std::cout << "Finished tests" << std::endl;
    } catch (std::exception& e) {
        std::cout << "Test failed due to: " << e.what() << std::endl;
        return -1;
    }
}

void startTests() {
    ServerSocketTest serverSocketTest{};
    serverSocketTest.startTest();
}