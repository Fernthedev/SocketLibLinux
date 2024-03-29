#pragma once

#include "Socket.hpp"
#include "SocketUtil.hpp"
#include <unordered_map>
#include <shared_mutex>
#include "ExclusiveSharedMutex.h"

#include "SocketLogger.hpp"
#include "fmt/format.h"

namespace SocketLib {

    class ServerSocket : public Socket {
    public:
        constexpr static const std::string_view SERVER_LOG_TAG = "server";

        explicit ServerSocket(SocketHandler *socketHandler, uint32_t id, uint32_t port)
                : Socket(socketHandler, id, std::nullopt, port) {}

        /// Binds to the port and starts listening
        void bindAndListen();

        void setupReuseAddr(int yes = 1) {
            // lose the pesky "Address already in use" error message
            if (int status = setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
                getLogger().fmtThrowError<std::invalid_argument>(SERVER_LOG_TAG, "There was an error: {}",
                                                                 gai_strerror(status));
            }
        }

        ~ServerSocket() override;

        /// Disconnects the client
        void closeClient(int clientDescriptor);

        sockaddr_storage getPeerName(int clientDescriptor);

        HostPort getPeerAddress(int clientDescriptor);

        // TODO:
        void getHostName() {};

        void getHostByName() {};

        /// This will write to the clientDescriptor
        /// Note: This is slower than Channel.queueWrite because it grabs a lock
        /// \param clientDescriptor
        /// \param msg
        void write(int clientDescriptor, const Message &msg);

        [[nodiscard]] std::unordered_map<int, std::unique_ptr<Channel>> const &getClients() const {
            return clientDescriptors;
        }

        // readThreads = workerThreadCount
        // writeThreads = workerThreadCount
        std::uint16_t workerThreadCount = 2;

    protected:
        void onConnectedClient(int clientDescriptor);

        void disconnectInternal(int clientDescriptor) override {
            closeClient(clientDescriptor);
        }

    private:
        /// Connection listen loop
        void connectionListenLoop();

        void writeLoop();

        void readLoop();

        std::thread connectionListenThread;

        ExclusivePrioritySharedMutex clientDescriptorsMutex;
        std::unordered_map<int, std::unique_ptr<Channel>> clientDescriptors;
        std::vector<std::thread> workerThreads;

        void threadLoop();
    };
}