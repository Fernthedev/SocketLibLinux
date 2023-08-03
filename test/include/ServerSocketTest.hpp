#pragma once

#include "ServerSocket.hpp"

namespace SocketLib {
    class ServerSocketTest {
    public:
        ServerSocketTest() = default;
        ~ServerSocketTest() = default;

        void startTest();
        void connectEvent(SocketLib::Channel& channel, bool connected) const;
        void listenOnEvents(SocketLib::Channel& clientDescriptor, SocketLib::ReadOnlyStreamQueue& incomingQueue) const;

        ServerSocket* serverSocket;
    };
}

