#pragma once

#include "ClientSocket.hpp"

namespace SocketLib {
    class ClientSocketTest {
    public:
        ClientSocketTest() = default;
        ~ClientSocketTest() = default;

        void startTest();
        void connectEvent(SocketLib::Channel& channel, bool connected) const;
        void listenOnEvents(SocketLib::Channel& clientDescriptor, SocketLib::ReadOnlyStreamQueue& incomingQueue) const;

        ClientSocket* clientSocket;
    };
}

