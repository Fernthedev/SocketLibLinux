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
    servInfo = Utils::resolveEndpoint(getLogger(), host ? host->c_str() : nullptr, std::to_string(port).c_str());

    Utils::throwIfError(getLogger(), servInfo, SOCKET_LOG_TAG);

    for (auto p = servInfo; p != nullptr; p = p->ai_next) {
        getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(SOCKET_LOG_TAG, "creating socket");
        socketDescriptor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (socketDescriptor == -1) {
            Utils::logIfError(getLogger(), socketDescriptor, "creating socket", SOCKET_LOG_TAG);
            continue;
        }

        servInfo = p;

        break;
    }

    Utils::throwIfError(getLogger(), servInfo, SOCKET_LOG_TAG);
}

SocketLib::Socket::~Socket() {
    getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(SOCKET_LOG_TAG, "Destroy!");
    if (!destroyed) {
        destroyed = true;

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

Logger &Socket::getLogger() {
    return socketHandler->getLogger();
}

Channel::Channel(Socket const &socket, Logger &logger, ListenEventCallback &listenCallback, int clientDescriptor) :
        clientDescriptor(clientDescriptor),
        active(true),
        socket(socket),
        logger(logger),
        listenCallback(listenCallback),
        writeConsumeToken(writeQueue) {
    this->writeThread = std::thread(&Channel::writeThreadLoop, this);
    this->readThread = std::thread(&Channel::readThreadLoop, this);
}

Logger &Channel::getLogger() {
    return logger;
}

void Channel::queueWrite(const Message &msg) {
    if (std::this_thread::get_id() == writeThread.get_id()) {
        // TODO: Immediately send only if queue empty? If not, queue up when there's work on the queue
        sendMessage(msg);
    } else {
        writeQueue.enqueue(msg);
    }
}

void Channel::readThreadLoop() {
    auto logToken = getLogger().createProducerToken();
    try {
        while (socket.isActive() && active) {
            auto bufferSize = socket.bufferSize;
            byte buf[bufferSize];

            long recv_bytes = recv(clientDescriptor, buf, bufferSize, 0);

            int err = errno;

            if (!isActive()) {
                break;
            }

            // Non blocking
            if (recv_bytes == -1 && err == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }

            if (recv_bytes == 0 || err == ECONNRESET) {
                this->queueShutdown();
                break;
                // Error
            }

            if (recv_bytes < 0) {
                this->queueShutdown();
                Utils::throwIfError<true>(getLogger(), err, CHANNEL_LOG_TAG);
                break;
                // Queue up remaining data
            }

            // Success
            Message message(buf, recv_bytes);

            if (!listenCallback.empty()) {
                listenCallback.invokeError(*this, message, [&logToken, this](auto const &e) constexpr {
                    getLogger().writeLog<LoggerLevel::ERROR>(logToken, CHANNEL_LOG_TAG,
                                                             fmt::format("Exception caught in listener: {}",
                                                                         e.what()));
                });
            }
        }
    } catch (std::exception const &e) {
        getLogger().fmtLog<LoggerLevel::ERROR>(logToken, CHANNEL_LOG_TAG,
                                               "Closing socket because it has crashed fatally while reading: {}",
                                               e.what());
        this->queueShutdown();
    } catch (...) {
        getLogger().writeLog<LoggerLevel::ERROR>(logToken, CHANNEL_LOG_TAG,
                                                 "Closing socket because it has crashed fatally while reading for an unknown reason");
        this->queueShutdown();
    }

    getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Read loop ending");
}

void Channel::writeThreadLoop() {
    auto logToken = getLogger().createProducerToken();

    try {
        while (socket.isActive() && active) {
            Message message(nullptr, 0);

            // TODO: Find a way to forcefully stop waiting for deque
            if (writeQueue.wait_dequeue_timed(writeConsumeToken, message, std::chrono::milliseconds(5))) {
                sendMessage(message);
                continue;
            }

            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    } catch (std::exception const &e) {
        getLogger().fmtLog<LoggerLevel::ERROR>(logToken, CHANNEL_LOG_TAG,
                                               "Closing socket because it has crashed fatally while writing: {}",
                                               e.what());
        this->queueShutdown();
    } catch (...) {
        getLogger().writeLog<LoggerLevel::ERROR>(logToken, CHANNEL_LOG_TAG,
                                                 "Closing socket because it has crashed fatally while writing for an unknown reason");
        this->queueShutdown();
    }
}

void Channel::sendMessage(const Message &message) {
    sendBytes(message);
}

void Channel::sendBytes(std::span<const byte> bytes) {
    // Throw exception here?
    if (!active || !socket.isActive()) {
        getLogger().writeLog<LoggerLevel::WARN>(CHANNEL_LOG_TAG, "Sending data when socket isn't active, why?");
        return;
    }

    long sent_bytes = 0;
    while (sent_bytes < bytes.size()) {
        if (!isActive()) break;

        sent_bytes = send(clientDescriptor, bytes.data(), bytes.size(), 0);
        int err = errno;

        // Queue up remaining data
        if (sent_bytes < 0) {
            this->queueShutdown();
            Utils::throwIfError<true>(getLogger(), err, CHANNEL_LOG_TAG);
            break;
        } else if (sent_bytes < bytes.size()) {
            bytes.subspan(sent_bytes) = bytes.subspan(sent_bytes);
        }
    }
}

Channel::~Channel() {
    queueShutdown();
    active = false;

    if (!deconstructMutex.try_lock()) {
        throw std::runtime_error("Channel is already being destroyed!");
    }

    std::lock_guard<std::mutex> lock2(deconstructMutex);

    if (readThread.joinable()) {
        getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Read ending");
        readThread.detach();
    }

    if (writeThread.joinable()) {
        getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Write ending");
        writeThread.join();
    }

    getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Ending read write thread");
}

bool Channel::isActive() const {
    return socket.isActive() && active;
}

/// When the owning socket sees the active false bool, the channel will be deleted
void Channel::queueShutdown() {
    active = false;
}


