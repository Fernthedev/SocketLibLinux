#include "ServerSocketTest.hpp"

#include <cstdio>
#include <iostream>
#include <csignal>

#include "SocketHandler.hpp"

using namespace SocketLib;

void SocketLib::ServerSocketTest::connectEvent(int clientDescriptor, bool connected) const {
    std::cout << "Connected " << clientDescriptor << " status: " << (connected ? "true" : "false") << std::endl;

    if (connected) {
        serverSocket->write(clientDescriptor, Message("hi!\n"));
    }
}

void SocketLib::ServerSocketTest::startTest() {
    std::cout << "Starting server at port 3306" << std::endl;
    SocketHandler& socketHandler = SocketHandler::getCommonSocketHandler();

    serverSocket = socketHandler.createServerSocket(3306);
    serverSocket->bindAndListen();
    std::cout << "Started server\n";

    ServerSocket& serverSocket = *this->serverSocket;

    serverSocket.registerConnectCallback([this](int client, bool connected){
        connectEvent(client, connected);
    });

    serverSocket.registerListenCallback([this](int client, const Message& message){
        listenOnEvents(client, message);
    });

    std::cout << "Listening server fully started up" << std::endl;

    // This is only to keep the test running.
    // When using as a lib, realistically you won't do this
    while (serverSocket.isActive()) {}

    std::cout << "Finished server test, awaiting for server shutdown" << std::endl;

    // The destructor will remove the socket from the SocketHandler automatically.
    // You can use a smart pointer like unique pointer to handle this for you
    delete this->serverSocket;
}

void ServerSocketTest::listenOnEvents(int client, const Message &message) {
    auto msgStr = message.toString();
    std::cout << "Received: " << msgStr << std::endl;

    if (msgStr.find("stop") != std::string::npos) {
        coutdebug << "Stopping server now!" << std::endl;

        // This is not the recommended way to fully stop the server
        // this only sets the variable to false so the server socket can set active to false
        // which will release the loop and delete the pointer
        serverSocket->notifyStop();
    }
}
