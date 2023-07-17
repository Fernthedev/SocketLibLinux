#include <iostream>
#include <netinet/tcp.h>
#include <unistd.h>


#include "SocketUtil.hpp"
#include "ServerSocket.hpp"
#include "SocketHandler.hpp"

#ifndef SOCKET_SERVER_BACKLOG
#define SOCKET_SERVER_BACKLOG 10     // how many pending connections queue will hold
#endif

#define serverLog(level, ...) getLogger().fmtLog<level>(SERVER_LOG_TAG, __VA_ARGS__)
#define serverErrorThrow(...) getLogger().fmtThrowError(SERVER_LOG_TAG, __VA_ARGS__)

using namespace SocketLib;

void ServerSocket::bindAndListen() {
    if (isActive()) {
        serverErrorThrow("Already running server!");
    }

    if (destroyed) {
        serverErrorThrow("Server already destroyed");
    }

    active = true;

    int yes = 1;
    // Forcefully attaching socket to the port 8080
    Utils::throwIfError(getLogger(), setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes, sizeof(yes)), SERVER_LOG_TAG);

    if (noDelay) {
        Utils::throwIfError(getLogger(),
                            setsockopt(socketDescriptor, IPPROTO_TCP,
                                       TCP_NODELAY, &yes, sizeof(yes)),
                            SERVER_LOG_TAG);
    }

    Utils::throwIfError(getLogger(), bind(socketDescriptor, servInfo->ai_addr, servInfo->ai_addrlen), SERVER_LOG_TAG);

    connectionListenThread = std::thread(&ServerSocket::connectionListenLoop, this);


//    if (int status = listen(socketDescriptor, BACKLOG) == -1) {
//        perror("listen");
//        Utils::throwIfError(status);
//    }
    Utils::throwIfError(getLogger(), listen(socketDescriptor, SOCKET_SERVER_BACKLOG), SERVER_LOG_TAG);
}


ServerSocket::~ServerSocket() {
    serverLog(LoggerLevel::DEBUG_LEVEL, "Deleting server socket");
    for (auto const& threadGroupPair : clientDescriptors) {
        auto& channel = threadGroupPair.second;
        closeClient(channel->clientDescriptor);
    }

    if (connectionListenThread.joinable())
        connectionListenThread.detach();

    if (socketDescriptor != -1) {
        int status = shutdown(socketDescriptor, SHUT_RDWR);

        Utils::logIfError(getLogger(), status, "Unable to queueShutdown", SERVER_LOG_TAG);
        status = close(socketDescriptor);

        Utils::logIfError(getLogger(), status, "Unable to close", SERVER_LOG_TAG);
        socketDescriptor = -1;
    }

    serverLog(LoggerLevel::DEBUG_LEVEL, "Finish deleting server socket");
}

void ServerSocket::onConnectedClient(int clientDescriptor) {
    std::unique_lock<std::shared_mutex> writeLock(clientDescriptorsMutex);
    auto channel = std::make_unique<Channel>(*this, getLogger(), listenCallback, clientDescriptor);

    auto* channelPtr = channel.get();

    clientDescriptors.emplace(clientDescriptor, std::move(channel));

    if (connectCallback.empty()) return;
    socketHandler->queueWork([channelPtr, this] {
        connectCallback.invoke(*channelPtr, true);
    });
}

void ServerSocket::write(int clientDescriptor, const Message& message) {
    std::shared_lock<std::shared_mutex> readLock(clientDescriptorsMutex);
    auto it = clientDescriptors.find(clientDescriptor);

    if (it == clientDescriptors.end()) {
        serverErrorThrow("Client descriptor {} does not exist", clientDescriptor);
    }
    auto& group = it->second;

    group->queueWrite(message);
}

void ServerSocket::closeClient(int clientDescriptor) {
    std::unique_lock<std::shared_mutex> writeLock(clientDescriptorsMutex);
    auto it = clientDescriptors.find(clientDescriptor);

    if (it == clientDescriptors.end()) {
        serverErrorThrow("Client descriptor {} does not exist", clientDescriptor);
    }

    Channel &channel = *it->second.get();
    if (!connectCallback.empty()) {
        // TODO: Catch exceptions?
        // TODO: Should we even use reference types here?
        connectCallback.invoke(channel, false);
    }

    // Calls ~Channel()
    clientDescriptors.erase(it);

    serverLog(LoggerLevel::DEBUG_LEVEL, "Client being deleted: {}", it->first);
    shutdown(clientDescriptor, SHUT_RDWR);
    close(clientDescriptor);

    serverLog(LoggerLevel::DEBUG_LEVEL, "Fully finished clearing client data from server memory.");
}

sockaddr_storage ServerSocket::getPeerName(int clientDescriptor) {
    struct sockaddr_storage their_addr{};
    socklen_t addr_size = sizeof their_addr;

    Utils::throwIfError(getLogger(), getpeername(clientDescriptor, (struct sockaddr *)&their_addr, &addr_size), SERVER_LOG_TAG);

    return their_addr;
}

HostPort ServerSocket::getPeerAddress(int clientDescriptor) {
    auto socketAddress = getPeerName(clientDescriptor);
    return Utils::getHostByAddress(getLogger(), socketAddress);
}

// TODO: Replace with epoll
void ServerSocket::connectionListenLoop() {
    while (isActive()) {
        this->threadLoop();

        struct sockaddr_storage their_addr{};
        socklen_t addr_size = sizeof their_addr;
        int new_fd = accept4(socketDescriptor, (struct sockaddr *) &their_addr, &addr_size, SOCK_NONBLOCK);

        auto err = errno;

        if (!isActive()) break;

        if (new_fd < 0) {
            // non block handle
            if (err == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::microseconds (100));
                continue;
            }

            Utils::logIfError(getLogger(), new_fd, "Failed to accept client", SERVER_LOG_TAG);
            continue;
        }

        onConnectedClient(new_fd);
    }
}

void ServerSocket::threadLoop() {
    for (auto const& [id, socket] : this->clientDescriptors) {
        if (socket->isActive()) continue;

        closeClient(socket->clientDescriptor);
    }
}

