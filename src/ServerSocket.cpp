#include <unistd.h>
#include <iostream>

#include "SocketUtil.hpp"
#include "ServerSocket.hpp"
#include "SocketHandler.hpp"

#ifndef SOCKET_SERVER_BACKLOG
#define SOCKET_SERVER_BACKLOG 10     // how many pending connections queue will hold
#endif

#define serverLog(level, ...) Logger::fmtLog<level>(SERVER_LOG_TAG, __VA_ARGS__)
#define serverErrorThrow(...) Logger::fmtThrowError(SERVER_LOG_TAG, __VA_ARGS__)

using namespace SocketLib;

void ServerSocket::bindAndListen() {
    if (isActive()) {
        serverErrorThrow("Already running server!");
    }

    if (destroyed) {
        serverErrorThrow("Server already destroyed");
    }

    active = true;

    int opt = 1;
    // Forcefully attaching socket to the port 8080
    Utils::throwIfError(setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)), SERVER_LOG_TAG);

    Utils::throwIfError(bind(socketDescriptor, servInfo->ai_addr, servInfo->ai_addrlen), SERVER_LOG_TAG);

    connectionListenThread = std::thread(&ServerSocket::connectionListenLoop, this);


//    if (int status = listen(socketDescriptor, BACKLOG) == -1) {
//        perror("listen");
//        Utils::throwIfError(status);
//    }
    Utils::throwIfError(listen(socketDescriptor, SOCKET_SERVER_BACKLOG), SERVER_LOG_TAG);
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

        Utils::logIfError(status, "Unable to shutdown", SERVER_LOG_TAG);
        status = close(socketDescriptor);

        Utils::logIfError(status, "Unable to close", SERVER_LOG_TAG);
        socketDescriptor = -1;
    }

    serverLog(LoggerLevel::DEBUG_LEVEL, "Finish deleting server socket");
}

void ServerSocket::onConnectedClient(int clientDescriptor) {
    std::unique_lock<std::shared_mutex> writeLock(clientDescriptorsMutex);
    auto channel = std::make_unique<Channel>(*this, clientDescriptor);

    auto* channelPtr = channel.get();

    clientDescriptors.emplace(clientDescriptor, std::move(channel));

    if (!connectCallback.empty()) {
        socketHandler->queueWork([channelPtr, this] {
            connectCallback.invoke(*channelPtr, true);
        });
    }
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

    serverLog(LoggerLevel::DEBUG_LEVEL, "Client being deleted: {}", it->first);
    shutdown(clientDescriptor, SHUT_RDWR);
    close(clientDescriptor);

    Channel &channel = *it->second.get();
    if (!connectCallback.empty()) {
        // TODO: Catch exceptions?
        // TODO: Should we even use reference types here?
        connectCallback.invoke(channel, false);
    }

    clientDescriptors.erase(it);


    // TODO: Somehow validate that there are no memory leaks here?
    serverLog(LoggerLevel::DEBUG_LEVEL, "Fully finished clearing client data from server memory.");
}

sockaddr_storage ServerSocket::getPeerName(int clientDescriptor) {
    struct sockaddr_storage their_addr{};
    socklen_t addr_size = sizeof their_addr;

    Utils::throwIfError(getpeername(clientDescriptor, (struct sockaddr *)&their_addr, &addr_size), SERVER_LOG_TAG);

    return their_addr;
}

HostPort ServerSocket::getPeerAddress(int clientDescriptor) {
    auto socketAddress = getPeerName(clientDescriptor);
    return Utils::getHostByAddress(socketAddress);
}

void ServerSocket::connectionListenLoop() {
    while (isActive()) {
        struct sockaddr_storage their_addr{};
        socklen_t addr_size = sizeof their_addr;
        int new_fd = accept(socketDescriptor, (struct sockaddr *) &their_addr, &addr_size);

        if (new_fd < 0) {
            Utils::logIfError(new_fd, "Failed to accept client", SERVER_LOG_TAG);
            continue;
        } else {
            onConnectedClient(new_fd);
        }
    }
}

