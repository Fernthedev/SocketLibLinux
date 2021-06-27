#pragma once

#include "ServerSocket.hpp"

namespace SocketLib {
    class ServerSocketTest {
    public:
        ServerSocketTest() = default;
        ~ServerSocketTest() = default;

        void startTest();
        void connectEvent(int clientDescriptor, bool connected) const;
        void listenOnEvents(int clientDescriptor, const Message& message);

        ServerSocket* serverSocket;
    };
}

