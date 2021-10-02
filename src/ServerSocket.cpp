#include <unistd.h>
#include <iostream>

#include "SocketUtil.hpp"
#include "ServerSocket.hpp"
#include "SocketHandler.hpp"

#ifndef SOCKET_SERVER_BACKLOG
#define SOCKET_SERVER_BACKLOG 10     // how many pending connections queue will hold
#endif

using namespace SocketLib;

void ServerSocket::bindAndListen() {
    if (isActive())
        throw std::runtime_error(std::string("Already running server!"));

    if (destroyed)
        throw std::runtime_error(std::string("Server already destroyed"));

    active = true;

    int opt = 1;
    // Forcefully attaching socket to the port 8080
    Utils::throwIfError(setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)));

    Utils::throwIfError(bind(socketDescriptor, servInfo->ai_addr, servInfo->ai_addrlen));

    connectionListenThread = std::thread(&ServerSocket::connectionListenLoop, this);


//    if (int status = listen(socketDescriptor, BACKLOG) == -1) {
//        perror("listen");
//        Utils::throwIfError(status);
//    }
    Utils::throwIfError(listen(socketDescriptor, SOCKET_SERVER_BACKLOG));
}


ServerSocket::~ServerSocket() {
    coutdebug << "Deleting server socket" << std::endl;
    for (auto const& threadGroupPair : clientDescriptors) {
        auto& channel = threadGroupPair.second;
        closeClient(channel->clientDescriptor);
    }

    if (connectionListenThread.joinable())
        connectionListenThread.detach();

    if (socketDescriptor != -1) {
        int status = shutdown(socketDescriptor, SHUT_RDWR);

        if (status != 0) {
            perror("Unable to close");
        }

        status = close(socketDescriptor);
        if (status != 0) {
            perror("Unable to close");
        }
        socketDescriptor = -1;
    }

    coutdebug << "Finish Deleting server socket" << std::endl;
}

void ServerSocket::onConnectedClient(int clientDescriptor) {
    std::unique_lock<std::shared_mutex> writeLock(clientDescriptorsMutex);
    std::unique_ptr<Channel> channel = std::make_unique<Channel>(*this, clientDescriptor);

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

    if (it == clientDescriptors.end())
        throw std::runtime_error(std::string("Client descriptor does not exist"));

    auto& group = it->second;

    group->queueWrite(message);
}

void ServerSocket::closeClient(int clientDescriptor) {
    std::unique_lock<std::shared_mutex> writeLock(clientDescriptorsMutex);
    auto it = clientDescriptors.find(clientDescriptor);

    if (it == clientDescriptors.end())
        throw std::runtime_error(std::string("Client descriptor %d does not exist", clientDescriptor));


    coutdebug << "Client being deleted:" << it->first << std::endl;
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
    coutdebug << "Fully finished clearing client data from server memory." << std::endl;
}

sockaddr_storage ServerSocket::getPeerName(int clientDescriptor) {
    struct sockaddr_storage their_addr{};
    socklen_t addr_size = sizeof their_addr;

    Utils::throwIfError(getpeername(clientDescriptor, (struct sockaddr *)&their_addr, &addr_size));

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
            perror("accept");
            continue;
        } else {
            onConnectedClient(new_fd);
        }
    }
}

