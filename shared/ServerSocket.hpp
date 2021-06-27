#pragma once

#include "Socket.hpp"
#include "SocketUtil.hpp"
#include <unordered_map>
#include <shared_mutex>

namespace SocketLib {

    class ServerSocket : public Socket {
    public:
        explicit ServerSocket(SocketHandler *socketHandler, uint32_t id, uint32_t port);

        /// Binds to the port and starts listening
        void bindAndListen();

        void setupReuseAddr(int yes=1) {
            // lose the pesky "Address already in use" error message
            if (int status = setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
                perror("setsockopt");
                throw std::invalid_argument("There was an error: " + std::string(gai_strerror(status)));
            }
        }

        ~ServerSocket() override;

        /// Disconnects the client
        void closeClient(int clientDescriptor);

        sockaddr_storage getPeerName(int clientDescriptor);
        HostPort getPeerAddress(int clientDescriptor);

        // TODO:
        void getHostName();
        void getHostByName();

        /// This will write to the clientDescriptor
        /// Note: This is slower than Channel.queueWrite because it grabs a lock
        /// \param clientDescriptor
        /// \param msg
        void write(int clientDescriptor, const Message& msg);

    protected:
        void onConnectedClient(int clientDescriptor) override;

        void onDisconnectClient(int clientDescriptor) override;

    private:
        /// Connection listen loop
        void connectionListenLoop();

        std::thread connectionListenThread;

        std::shared_mutex clientDescriptorsMutex;
        std::unordered_map<int, std::shared_ptr<Channel>> clientDescriptors;
    };
}