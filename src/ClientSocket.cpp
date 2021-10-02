#include <unistd.h>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "SocketUtil.hpp"
#include "ClientSocket.hpp"
#include "SocketHandler.hpp"

using namespace SocketLib;

ClientSocket::ClientSocket(SocketHandler *socketHandler, uint32_t id, const std::string &address, uint32_t port)
        : Socket(socketHandler, id, address, port) {
    char s[INET6_ADDRSTRLEN];
    inet_ntop(servInfo->ai_family, Utils::get_in_addr((struct sockaddr *)servInfo->ai_addr), s, sizeof s);

    coutdebug << "Connecting to " << s << std::endl;
}


void ClientSocket::connect() {
    if (::connect(socketDescriptor, servInfo->ai_addr, servInfo->ai_addrlen) == -1) {
        ::close(socketDescriptor);
        perror("client: connect");
        throw std::runtime_error(std::string("Failed to connect"));
    }


    channel = std::make_unique<Channel>(*this, socketDescriptor);


    if (!connectCallback.empty()) {
        socketHandler->queueWork([this] {
            connectCallback.invoke(*channel, true);
        });
    }
}

ClientSocket::~ClientSocket() {
    coutdebug << "Deleting client socket" << std::endl;

    close();
    coutdebug << "Finish Deleting client socket" << std::endl;
}

void ClientSocket::close() {
    if (socketDescriptor != -1) {
        int status = shutdown(socketDescriptor, SHUT_RDWR);

        if (status != 0) {
            perror("Unable to shutdown client");
        }

        status = ::close(socketDescriptor);
        if (status != 0) {
            perror("Unable to close client");
        }

        socketDescriptor = -1;
    }

    Channel &channelRef = *channel;
    if (!connectCallback.empty()) {
        // TODO: Catch exceptions?
        // TODO: Should we even use reference types here?
        connectCallback.invoke(channelRef, false);
    }

    coutdebug << "Closed client socket" << std::endl;

    // TODO: Somehow validate that there are no memory leaks here?
}
