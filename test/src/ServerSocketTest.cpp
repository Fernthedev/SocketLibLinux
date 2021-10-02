#include "ServerSocketTest.hpp"

#include <cstdio>
#include <iostream>
#include <csignal>
#include <thread>

#include "SocketHandler.hpp"

using namespace SocketLib;

void SocketLib::ServerSocketTest::connectEvent(Channel& channel, bool connected) const {
    std::cout << "Connected " << channel.clientDescriptor << " status: " << (connected ? "true" : "false") << std::endl;

    if (connected) {
        channel.queueWrite(Message("hi!\n"));
    }
}

void SocketLib::ServerSocketTest::startTest() {
    std::cout << "Starting server at port 3306" << std::endl;
    SocketHandler& socketHandler = SocketHandler::getCommonSocketHandler();

    serverSocket = socketHandler.createServerSocket(3306);
    serverSocket->bindAndListen();
    std::cout << "Started server\n";

    ServerSocket& serverSocket = *this->serverSocket;

    serverSocket.connectCallback += [this](Channel& client, bool connected){
        connectEvent(client, connected);
    };

    serverSocket.listenCallback += [this](Channel& client, const Message& message){
        listenOnEvents(client, message);
    };

    std::cout << "Listening server fully started up" << std::endl;

    // This is only to keep the test running.
    // When using as a lib, realistically you won't do this
    while (serverSocket.isActive()) {
        std::this_thread::yield();
    }

    std::cout << "Finished server test, awaiting for server shutdown" << std::endl;

    // The destructor will remove the socket from the SocketHandler automatically.
    // You can use a smart pointer like unique pointer to handle this for you
    delete this->serverSocket;
}

void ServerSocketTest::listenOnEvents(Channel& client, const Message &message) const {
    auto msgStr = message.toString();
    std::cout << "Received: " << msgStr << std::endl;

    if (msgStr.find("stop") != std::string::npos) {
        coutdebug << "Stopping server now!" << std::endl;

        // This is not the recommended way to fully stop the server
        // this only sets the variable to false so the server socket can set active to false
        // which will release the loop and delete the pointer
        serverSocket->notifyStop();
        return;
    }

    // Construct message
    std::stringstream messageToOthers("Client ");

    messageToOthers << std::to_string(client.clientDescriptor) << ": " << msgStr;

    Message constructedMessage(messageToOthers.str());

    // Forward message to other clients if any
    if (serverSocket->getClients().size() > 1) {
        for (auto& [id, client2]: serverSocket->getClients()) {
            if (client.clientDescriptor == client2->clientDescriptor)
                continue;


            client2->queueWrite(constructedMessage);
        }
    }
}
