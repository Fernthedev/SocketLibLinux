#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "SocketUtil.hpp"
#include "ClientSocket.hpp"
#include "SocketHandler.hpp"
#include "SocketLogger.hpp"

#include "fmt/format.h"

#define clientLog(level, ...) getLogger().fmtLog<level>(CLIENT_LOG_TAG, __VA_ARGS__)
#define clientErrorThrow(...) getLogger().fmtThrowError(CLIENT_LOG_TAG, __VA_ARGS__)

using namespace SocketLib;

ClientSocket::ClientSocket(SocketHandler *socketHandler, uint32_t id, const std::string &address, uint32_t port)
        : Socket(socketHandler, id, address, port) {
    char s[INET6_ADDRSTRLEN];
    inet_ntop(servInfo->ai_family, Utils::get_in_addr((struct sockaddr *) servInfo->ai_addr), s, sizeof s);

    clientLog(LoggerLevel::DEBUG_LEVEL, "Connecting to {}", s);
}


void ClientSocket::connect() {
    int yes = 1;
    if (noDelay) {
        Utils::throwIfError(getLogger(),
                            setsockopt(socketDescriptor, IPPROTO_TCP, TCP_NODELAY,
                                       &yes, sizeof(yes)),
                            CLIENT_LOG_TAG);
    }

    while (true) {
        auto connectStatus = ::connect(socketDescriptor, servInfo->ai_addr, servInfo->ai_addrlen);
        auto err = errno;
        if (connectStatus == 0) {
            break;
        }

        // non block handle
        if (err == EWOULDBLOCK) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        ::close(socketDescriptor);
        clientErrorThrow("Failed to connect");
        break;
    }

    active = true;
    channel = std::make_unique<Channel>(*this, getLogger(), listenCallback, socketDescriptor);

    if (connectCallback.empty()) {
        return;
    }
    std::thread([this] {
        connectCallback.invoke(*channel, true);
    }).detach();
}

ClientSocket::~ClientSocket() {
    clientLog(LoggerLevel::DEBUG_LEVEL, "Deleting client socket");

    close();
    clientLog(LoggerLevel::DEBUG_LEVEL, "Finish deleting client socket");
}

void ClientSocket::close() {
    if (socketDescriptor != -1) {
        int status = shutdown(socketDescriptor, SHUT_RDWR);

        if (status != 0) {
            clientLog(LoggerLevel::ERROR, "Unable to shut down client");
        }

        status = ::close(socketDescriptor);
        if (status != 0) {
            clientLog(LoggerLevel::ERROR, "Unable to close client");
        }

        socketDescriptor = -1;
    }

    Channel &channelRef = *channel;
    if (!connectCallback.empty()) {
        // TODO: Catch exceptions?
        // TODO: Should we even use reference types here?
        connectCallback.invoke(channelRef, false);
    }

    clientLog(LoggerLevel::DEBUG_LEVEL, "Closed client socket");

    // TODO: Somehow validate that there are no memory leaks here?
}

void ClientSocket::readLoop() {
    byte buf[bufferSize];
    std::span byteSpan(buf, bufferSize);

    auto logToken = this->getLogger().createProducerToken();
    while (isActive()) {
        auto sleepTime = std::chrono::microseconds(500);

        bool foundReadableChannel = channel->readData(byteSpan, logToken);

        if (!foundReadableChannel) {
            std::this_thread::yield();
            std::this_thread::sleep_for(sleepTime);
        }
    }
}

void ClientSocket::writeLoop() {
    auto logToken = this->getLogger().createProducerToken();
    while (isActive()) {
        auto sleepTime = std::chrono::microseconds(500);

        bool foundReadableChannel = channel->handleWriteQueue(logToken);

        if (!foundReadableChannel) {
            std::this_thread::yield();
            std::this_thread::sleep_for(sleepTime);
        }
    }
}
