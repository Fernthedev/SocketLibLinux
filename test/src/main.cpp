#include "main.hpp"
#include "ServerSocketTest.hpp"

#include <iostream>
#include <exception>

using namespace SocketLib;

int main() {
//#ifdef __GNUC__
//    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
//#endif

    try {
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