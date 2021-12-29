#include "Socket.hpp"

#include "SocketHandler.hpp"
#include "SocketUtil.hpp"
#include "SocketLogger.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>

#include <span>
#include <utility>

using namespace SocketLib;

SocketLib::Socket::Socket(SocketHandler *socketHandler, uint32_t id, std::optional<std::string> address, uint32_t port)
        : id(id), socketHandler(socketHandler), host(std::move(address)), port(port) {
    servInfo = Utils::resolveEndpoint(host ? host->c_str() : nullptr, std::to_string(port).c_str());

    Utils::throwIfError(servInfo, SOCKET_LOG_TAG);

    for (auto p = servInfo; p != nullptr; p = p->ai_next) {
        Logger::writeLog(LoggerLevel::DEBUG_LEVEL, SOCKET_LOG_TAG, "creating socket");
        socketDescriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (socketDescriptor == -1) {
            Utils::logIfError(socketDescriptor, "creating socket", SOCKET_LOG_TAG);
            continue;
        }

        servInfo = p;

        break;
    }

    Utils::throwIfError(servInfo, SOCKET_LOG_TAG);
}

SocketLib::Socket::~Socket() {
    Logger::writeLog(LoggerLevel::DEBUG_LEVEL, SOCKET_LOG_TAG, "Destroy!");
    if (!destroyed) {
        destroyed = true;
        socketHandler->destroySocket(id);

        if (servInfo) {
            freeaddrinfo(servInfo);
            servInfo = nullptr;
        }
    }
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

SocketHandler *Socket::getSocketHandler() const {
    return socketHandler;
}

Channel::Channel(Socket &socket, int clientDescriptor):
            clientDescriptor(clientDescriptor),
            active(true),
            socket(socket),
            writeConsumeToken(writeQueue) {
    this->writeThread = std::thread(&Channel::writeThreadLoop, this);
    this->readThread = std::thread(&Channel::readThreadLoop, this);
}

void Channel::queueWrite(const Message &msg) {
    if (std::this_thread::get_id() == writeThread.get_id()) {
        // TODO: Immediately send only if queue empty? If not, queue up when there's work on the queue
        sendData(msg);
    } else {
        writeQueue.enqueue(msg);
        Logger::writeLog(LoggerLevel::DEBUG_LEVEL, CHANNEL_LOG_TAG, "Queued write data");
    }
}

void Channel::readThreadLoop() {
    try {
        while (socket.isActive() && active) {
            auto bufferSize = socket.bufferSize;
            byte buf[bufferSize];


            long recv_bytes = recv(clientDescriptor, buf, bufferSize, 0);
            int err = errno;

            if (!active || !socket.isActive())
                continue;

            if (recv_bytes == 0 || err == ECONNRESET) {
                socket.disconnectInternal(clientDescriptor);

                return;

                // Error
            } else if (recv_bytes < 0) {
                socket.disconnectInternal(clientDescriptor);
                Utils::throwIfError<true>(err, CHANNEL_LOG_TAG);
                // Queue up remaining data
            } else {
                // Success
                byte receivedBuf[recv_bytes];

                std::copy(buf, buf + recv_bytes, receivedBuf);

                Message message(receivedBuf, recv_bytes);

                if (!socket.listenCallback.empty()) {
                    socket.listenCallback.invoke(*this, message);
                }
            }
        }
    } catch (std::exception &e) {
        Logger::writeLog(LoggerLevel::ERROR, CHANNEL_LOG_TAG, fmt::format("Closing socket because it has crashed fatally while reading: {}", e.what()));
        socket.disconnectInternal(clientDescriptor);
    } catch (...) {
        Logger::writeLog(LoggerLevel::ERROR, CHANNEL_LOG_TAG, "Closing socket because it has crashed fatally while reading for an unknown reason");
        socket.disconnectInternal(clientDescriptor);
    }

    Logger::writeLog(LoggerLevel::DEBUG_LEVEL, CHANNEL_LOG_TAG, "Read loop ending");
}

void Channel::writeThreadLoop() {
    try {
        while (socket.isActive() && active) {
            Message message(nullptr, 0);

            // TODO: Find a way to forcefully stop waiting for deque
            if (writeQueue.wait_dequeue_timed(writeConsumeToken, message, std::chrono::milliseconds(5))) {
                sendData(message);
            } else {
                std::this_thread::yield();
            }
        }
    } catch (std::exception const& e) {
        Logger::writeLog(LoggerLevel::ERROR, CHANNEL_LOG_TAG, fmt::format("Closing socket because it has crashed fatally while writing: {}", e.what()));
        socket.disconnectInternal(clientDescriptor);

    } catch (...) {
        Logger::writeLog(LoggerLevel::ERROR, CHANNEL_LOG_TAG, "Closing socket because it has crashed fatally while writing for an unknown reason");
        socket.disconnectInternal(clientDescriptor);
    }
}

void Channel::sendData(const Message &message) {
    // Throw exception here?
    if (!active || !socket.isActive()) {
        Logger::writeLog(LoggerLevel::WARN, CHANNEL_LOG_TAG, "Sending data when socket isn't active, why?");
        return;
    }

    long sent_bytes = send(clientDescriptor, message.data(), message.length(), 0);
    int err = errno;

    // Queue up remaining data
    if (sent_bytes < message.length()) {
        long startIndex = sent_bytes;

        size_t length = message.length() - sent_bytes;
        std::span<byte> remainingBytes(message.data() + startIndex, message.data() + startIndex + length);

        sendData(Message(remainingBytes));
    } else if (sent_bytes < 0) {
        socket.disconnectInternal(clientDescriptor);
        Utils::throwIfError<true>(err, CHANNEL_LOG_TAG);
    }
}

Channel::~Channel() {
    // TODO: Throw exception?
    active = false;

    std::lock_guard<std::mutex> lock2(deconstructMutex);

    if (readThread.joinable()) {
        Logger::writeLog(LoggerLevel::DEBUG_LEVEL, CHANNEL_LOG_TAG, "Read ending");
        readThread.detach();
    }

    if (writeThread.joinable()) {
        Logger::writeLog(LoggerLevel::DEBUG_LEVEL, CHANNEL_LOG_TAG, "Write ending");
        writeThread.join();
    }

    Logger::writeLog(LoggerLevel::DEBUG_LEVEL, CHANNEL_LOG_TAG, "Ending read write thread");
}


