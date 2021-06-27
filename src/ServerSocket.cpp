#include <SocketUtil.hpp>
#include <unistd.h>
#include <iostream>

#include "ServerSocket.hpp"
#include "SocketHandler.hpp"

#define BACKLOG 10     // how many pending connections queue will hold

using namespace SocketLib;

ServerSocket::ServerSocket(SocketHandler *socketHandler, uint32_t id, uint32_t port) : Socket(socketHandler, id, std::nullopt, port) {

}

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
    Utils::throwIfError(listen(socketDescriptor, BACKLOG));
}


ServerSocket::~ServerSocket() {
    coutdebug << "Deleting server socket" << std::endl;
    for (auto& threadGroupPair : clientDescriptors) {
        auto& group = threadGroupPair.second;
        closeClient(group->clientDescriptor);
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
    }

    coutdebug << "Finish Deleting server socket" << std::endl;
}

void ServerSocket::onConnectedClient(int clientDescriptor) {
    std::unique_lock<std::shared_mutex> writeLock(clientDescriptorsMutex);
    /// Is there a better way instead of lambda?
    std::unique_ptr<Channel> channel = std::make_unique<Channel>(*this, clientDescriptor, [this](int c) { onDisconnectClient(c); },
                                             getListenCallbacks());

    auto* channelPtr = channel.get();

    clientDescriptors.emplace(clientDescriptor, std::move(channel));

    if (!getConnectCallbacks().empty()) {
        socketHandler->queueWork([=] {
            for (const auto &callback : getConnectCallbacks()) {
                // TODO: Catch exceptions?
                // TODO: Should we even use reference types here?
                callback(*channelPtr, true);
            }
        });
    }
}

void ServerSocket::onDisconnectClient(int clientDescriptor) {
    std::shared_lock<std::shared_mutex> readLock(clientDescriptorsMutex);
    auto it = clientDescriptors.find(clientDescriptor);
    readLock.unlock();

    if (it == clientDescriptors.end())
        throw std::runtime_error(std::string("Client descriptor %d does not exist", clientDescriptor));

    Channel* channel = it->second.get();
    // This has to be done to ensure we don't call the destructor when a Channel calls this
    socketHandler->queueWork([=] {
        closeClient(clientDescriptor);
        if (!getConnectCallbacks().empty()) {
            for (const auto &callback : getConnectCallbacks()) {
                // TODO: Catch exceptions?
                // TODO: Should we even use reference types here?
                callback(*channel, false);
            }
        }
    });
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

    if (it != clientDescriptors.end()) {
        coutdebug << "Client being deleted:" << it->first << std::endl;
        shutdown(clientDescriptor, SHUT_RDWR);
        close(clientDescriptor);
        auto& group = it->second;
        clientDescriptors.erase(it);

        // TODO: Somehow validate that there are no memory leaks here?
    } else {
        shutdown(clientDescriptor, SHUT_RDWR);
        close(clientDescriptor);
    }

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

