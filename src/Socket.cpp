#include "Socket.hpp"

#include "SocketHandler.hpp"
#include "SocketUtil.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>

#include <utility>

using namespace SocketLib;

SocketLib::Socket::Socket(SocketHandler* socketHandler, uint32_t id, std::optional<std::string> address, uint32_t port) : socketHandler(socketHandler), id(id), host(std::move(address)),  port(port) {
    servInfo = Utils::resolveEndpoint(address ? address->c_str() : nullptr, std::to_string(port).c_str());

    Utils::throwIfError(servInfo);

    for (auto p = servInfo; p != nullptr; p = p->ai_next) {
        coutdebug << "creating socket" << std::endl;
        socketDescriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (socketDescriptor == -1) {
            perror("server: socket");
            continue;
        }

        servInfo = p;

        break;
    }
}

SocketLib::Socket::~Socket() {
    coutdebug << "Destroy!" << std::endl;
    if (!destroyed) {
        destroyed = true;
        socketHandler->destroySocket(id);

        if (servInfo) {
            freeaddrinfo(servInfo);
            servInfo = nullptr;
        }
    }
}

const std::vector<ListenCallback> &SocketLib::Socket::getListenCallbacks() const {
    return listenCallbacks;
}

void Socket::registerListenCallback(const ListenCallback &callback) {
    std::unique_lock<std::mutex> lock(callbackMutex);
    listenCallbacks.push_back(callback);
}

void Socket::clearListenCallbacks() noexcept {
    std::unique_lock<std::mutex> lock(callbackMutex);
    listenCallbacks.clear();
}

const std::vector<ConnectCallback> &Socket::getConnectCallbacks() const {
    return connectCallbacks;
}

void Socket:: registerConnectCallback(const ConnectCallback &callback) {
    std::unique_lock<std::mutex> lock(connectMutex);
    connectCallbacks.push_back(callback);
}

void Socket::clearConnectCallbacks() noexcept {
    std::unique_lock<std::mutex> lock(connectMutex);
    connectCallbacks.clear();
}


bool Socket::isActive() const {
    return active;
}

void Socket::notifyStop() {
    active = false;
}

bool Socket::isDestroyed() const {
    return destroyed;
}

Socket::ReadSendThreads::ReadSendThreads(Socket& socket, int clientDescriptor) : socket(socket), clientDescriptor(clientDescriptor),active(true),
                                                                                 writeConsumeToken(writeQueue) {
    this->writeThread = std::thread(&ReadSendThreads::writeThreadLoop, this);
    this->readThread = std::thread(&ReadSendThreads::readThreadLoop, this);
}

void Socket::ReadSendThreads::queueWrite(const Message& msg) {
    if (std::this_thread::get_id() == writeThread.get_id()) {
        // TODO: Immediately send only if queue empty? If not, queue up when there's work on the queue
        sendData(msg);
    } else {
        writeQueue.enqueue(msg);
        coutdebug << "Queued" << std::endl;
    }
}

void Socket::ReadSendThreads::readThreadLoop() {
    try {
        while (socket.isActive() && active) {
            auto bufferSize = socket.bufferSize;
            byte buf[bufferSize];


            long recv_bytes = recv(clientDescriptor, buf, bufferSize, 0);

            if (!active || !socket.isActive())
                continue;

            if (recv_bytes == 0) {
                socket.onDisconnectClient(clientDescriptor);

                return;

                // Error
            } else if (recv_bytes < 0) {
                socket.onDisconnectClient(clientDescriptor);
                Utils::throwIfError((int) recv_bytes);
                // Queue up remaining data
            } else {
                // Success
                byte receivedBuf[recv_bytes];

                std::copy(buf, buf + recv_bytes, receivedBuf);

                Message message(receivedBuf, recv_bytes);

                for (const auto &listener : socket.listenCallbacks) {
                    socket.socketHandler->queueWork([=] {
                        listener(clientDescriptor, message);
                    });
                }
            }
        }
    } catch(std::exception& e) {
        fprintf(stderr, "Closing socket because it has crashed fatally while reading: %s", e.what());
        socket.onDisconnectClient(clientDescriptor);
    } catch (...) {
        fprintf(stderr, "Closing socket because it has crashed fatally while reading for an unknown reason");
        socket.onDisconnectClient(clientDescriptor);
    }

    coutdebug << "Read loop ending" << std::endl;
}

void Socket::ReadSendThreads::writeThreadLoop() {
    try {
        while (socket.isActive() && active) {

            Message message(nullptr, 0);

            // TODO: Find a way to forcefully stop waiting for deque
            if (writeQueue.wait_dequeue_timed(writeConsumeToken, message, std::chrono::milliseconds(5))) {
                sendData(message);
            } else continue;
        }
    } catch(std::exception& e) {
        fprintf(stderr, "Closing socket because it has crashed fatally while writing: %s", e.what());
        socket.onDisconnectClient(clientDescriptor);

    } catch (...) {
        fprintf(stderr, "Closing socket because it has crashed fatally while writing for an unknown reason");
        socket.onDisconnectClient(clientDescriptor);
    }
}

void Socket::ReadSendThreads::sendData(const Message& message) {
    long sent_bytes = send(clientDescriptor, message.data(), message.length(), 0);

    // Queue up remaining data
    if (sent_bytes < message.length()) {
        long startIndex = sent_bytes;

        size_t length = message.length() - sent_bytes;
        byte *remainingBytes = new byte[length];


        std::copy(message.data() + startIndex, message.data() + startIndex + length, remainingBytes);

        queueWrite(Message(remainingBytes, length));
        delete[] remainingBytes;
    } else if (sent_bytes < 0) {
        socket.onDisconnectClient(clientDescriptor);
        Utils::throwIfError((int) sent_bytes);
    }
}

Socket::ReadSendThreads::~ReadSendThreads() {
    // TODO: Throw exception?
    active = false;

    std::lock_guard<std::mutex> lock2(deconstructMutex);

    if (readThread.joinable()) {
        coutdebug << "Read ending" << std::endl;
        readThread.detach();
    }

    if (writeThread.joinable()) {
        coutdebug << "Write ending" << std::endl;
        writeThread.join();
    }

    coutdebug << "Ending read write thread" << std::endl;
}


