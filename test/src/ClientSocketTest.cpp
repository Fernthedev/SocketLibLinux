#include "main.hpp"
#include "ClientSocketTest.hpp"

#include <cstdio>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

#include "SocketHandler.hpp"
#include "SocketLogger.hpp"

constexpr static const std::string_view TEST_LOG_TAG = "ClientSocketTest";

#define log(level, ...) getLogger().fmtLog<level>(TEST_LOG_TAG, __VA_ARGS__)

using namespace SocketLib;

void SocketLib::ClientSocketTest::connectEvent(Channel& channel, bool connected) const {
    log(LoggerLevel::INFO, "Connected {} status: {}", channel.clientDescriptor, connected ? "true" : "false");


    if (connected) {
        channel.queueWrite(Message("hi!\n"));
    }
}

void SocketLib::ClientSocketTest::startTest() {
    log(LoggerLevel::INFO, "Starting client at port 3506");
    SocketHandler& socketHandler = SocketHandler::getCommonSocketHandler();

    clientSocket = socketHandler.createClientSocket("0.0.0.0", 3506);
    clientSocket->connect();
    log(LoggerLevel::INFO, "Started client");

    ClientSocket& clientSocket = *this->clientSocket;

    clientSocket.connectCallback += [this](Channel& client, bool connected){
        connectEvent(client, connected);
    };

    clientSocket.listenCallback += [this](Channel& client, SocketLib::ReadOnlyStreamQueue& incomingQueue){
        listenOnEvents(client, incomingQueue);
    };

    log(LoggerLevel::INFO, "Listening client fully started up");

    // This is only to keep the test running.
    // When using as a lib, realistically you won't do this
    auto start = std::chrono::high_resolution_clock::now();
    while (clientSocket.isActive()) {
        auto now = std::chrono::high_resolution_clock::now();

        // get the elapsed time
        auto diff = now - start;

        // convert from the clock rate to a millisecond clock
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(diff);

        if (seconds.count() > 2) {
            start = now;
            clientSocket.write(Message("hi!"));
            log(LoggerLevel::INFO, "Sent hi!");
        }

        std::this_thread::yield();
    }

    log(LoggerLevel::INFO, "Finished client test, awaiting for client queueShutdown");

    // The destructor will remove the socket from the SocketHandler automatically.
    // You can use a smart pointer like unique pointer to handle this for you
    socketHandler.destroySocket(this->clientSocket);
}

void ClientSocketTest::listenOnEvents(Channel& client, SocketLib::ReadOnlyStreamQueue& incomingQueue) const {
    auto msgStr = incomingQueue.dequeAsMessage().toString();
    log(LoggerLevel::INFO, "Received: {}", msgStr.substr(0, msgStr.length() - 1));

    if (msgStr.find("stop") != std::string::npos) {
        log(LoggerLevel::INFO, "Stopping client now!");

        // This is not the recommended way to fully stop the client
        // this only sets the variable to false so the client socket can set active to false
        // which will release the loop and delete the pointer
        clientSocket->notifyStop();
        return;
    }

    // Construct message
    Message constructedMessage(fmt::format("Client {}: {}", client.clientDescriptor, msgStr));

    // Forward message to other clients if any
    clientSocket->write(constructedMessage);
}
