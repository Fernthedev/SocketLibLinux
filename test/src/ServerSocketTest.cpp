#include "main.hpp"
#include "ServerSocketTest.hpp"

#include <thread>

#include "SocketHandler.hpp"
#include "SocketLogger.hpp"

constexpr static const std::string_view TEST_LOG_TAG = "ServerSocketTest";

#define log(level, ...) getLogger().fmtLog<level>(TEST_LOG_TAG, __VA_ARGS__)

using namespace SocketLib;

void SocketLib::ServerSocketTest::connectEvent(Channel& channel, bool connected) const {
    log(LoggerLevel::INFO, "Connected {} status: {}", channel.clientDescriptor, connected ? "true" : "false");


    if (connected) {
        channel.queueWrite(Message("hi!\n"));
    }
}

void SocketLib::ServerSocketTest::startTest() {
    log(LoggerLevel::INFO, "Starting server at port 3306");
    SocketHandler& socketHandler = SocketHandler::getCommonSocketHandler();

    serverSocket = socketHandler.createServerSocket(3306);
    serverSocket->bindAndListen();
    log(LoggerLevel::INFO, "Started server");

    ServerSocket& serverSocket = *this->serverSocket;

    serverSocket.connectCallback += [this](Channel& client, bool connected){
        connectEvent(client, connected);
    };

    serverSocket.listenCallback += [this](Channel& client, const Message& message){
        listenOnEvents(client, message);
    };

    log(LoggerLevel::INFO, "Listening server fully started up");

    // This is only to keep the test running.
    // When using as a lib, realistically you won't do this
    while (serverSocket.isActive()) {
        std::this_thread::yield();
    }

    log(LoggerLevel::INFO, "Finished server test, awaiting for server shutdown");

    // The destructor will remove the socket from the SocketHandler automatically.
    // You can use a smart pointer like unique pointer to handle this for you
    socketHandler.destroySocket(this->serverSocket);
}

void ServerSocketTest::listenOnEvents(Channel& client, const Message &message) const {
    auto msgStr = message.toString();
    log(LoggerLevel::INFO, "Received: {}", msgStr);

    if (msgStr.find("stop") != std::string::npos) {
        log(LoggerLevel::INFO, "Stopping server now!");

        // This is not the recommended way to fully stop the server
        // this only sets the variable to false so the server socket can set active to false
        // which will release the loop and delete the pointer
        serverSocket->notifyStop();
        return;
    }

    // Construct message
    Message constructedMessage(fmt::format("Client {}: {}\n", client.clientDescriptor, msgStr));

    // Forward message to other clients if any
    if (serverSocket->getClients().size() > 1) {
        for (auto& [id, client2]: serverSocket->getClients()) {
            if (client.clientDescriptor == client2->clientDescriptor)
                continue;


            client2->queueWrite(constructedMessage);
        }
    }
}
