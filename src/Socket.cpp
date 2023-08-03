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
#include <fcntl.h>

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

        // Set socket to non-blocking mode
        int flags = fcntl(socketDescriptor, F_GETFL, 0);
        Utils::throwIfError(getLogger(),
                            fcntl(socketDescriptor, F_SETFL, flags | O_NONBLOCK),
                            SOCKET_LOG_TAG);

        servInfo = p;

        break;
    }

    Utils::throwIfError(getLogger(), servInfo, SOCKET_LOG_TAG);
}

SocketLib::Socket::~Socket() {
    getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(SOCKET_LOG_TAG, "Destroy!");
    if (servInfo != nullptr) {
        freeaddrinfo(servInfo);
        servInfo = nullptr;
    }
}


bool Socket::isActive() const {
    return active;
}

void Socket::notifyStop() {
    active = false;
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
}

Logger &Channel::getLogger() {
    return logger;
}

void Channel::queueWrite(const Message &msg) {
    if (!active) {
        return;
    }

    if (msg.data() == nullptr || msg.length() == 0) {
        return;
    }
    if (std::this_thread::get_id() != writeThread.get_id()) {
        writeQueue.enqueue(msg);
        return;
    }


    // TODO: Immediately send only if queue empty? If not, queue up when there's work on the queue
    sendMessage(msg);
}

bool Channel::readData(std::span<byte> byteBuf, moodycamel::ProducerToken const &logToken) {
    if (!active) {
        return false;
    }
    if (!readLock.try_lock()) {
        return false;
    }

    // don't lock since try_lock already locked
    std::lock_guard lock(readLock, std::adopt_lock);
    try {
        long recv_bytes = recv(clientDescriptor, byteBuf.data(), byteBuf.size(), MSG_DONTWAIT);

        int err = errno;

        if (!isActive()) {
            return false;
        }

        // End of socket
        if (recv_bytes == 0) {
            this->queueShutdown();
            return false;
            // Error
        }

        // Non blocking
        if (recv_bytes < 0) {
            // Connection reset
            if (err == ECONNRESET) {
                this->queueShutdown();
                return false;
            }

            if (err == EWOULDBLOCK) {
                return false;
            }

            this->queueShutdown();
            Utils::throwIfError<true>(getLogger(), err, CHANNEL_LOG_TAG);
            return false;
            // Queue up remaining data
        }

        // Success
        // offset 0 len recv_bytes
        Message message(byteBuf.subspan(0, recv_bytes));

        if (!listenCallback.empty()) {
            listenCallback.invokeError(*this, message, [&logToken, this](auto const &e) constexpr {
                getLogger().writeLog<LoggerLevel::ERROR>(logToken, CHANNEL_LOG_TAG,
                                                         fmt::format("Exception caught in listener: {}",
                                                                     e.what()));
            });
        }

        // success got data
        return true;
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

    return false;
}

bool Channel::handleWriteQueue(moodycamel::ProducerToken const &logToken) {
    if (!active) {
        return false;
    }

    // don't lock since try_lock already locked
    std::unique_lock lock(writeLock, std::try_to_lock);

    if (!lock) {
        return false;
    }

    try {
        auto constexpr messageReserve = 10;
        Message messages[messageReserve];


        auto dequeCount = writeQueue.wait_dequeue_bulk_timed(writeConsumeToken, messages, messageReserve,
                                                             std::chrono::microseconds (500));
        if (dequeCount == 0) {
            return false;
        }

        for (size_t i = 0; i < dequeCount; i++) {
            sendMessage(messages[i]);
        }

        return true;
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

    return false;
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
        if (!isActive()) {
            break;
        }

        sent_bytes = send(clientDescriptor, bytes.data(), bytes.size(), MSG_DONTWAIT);
        int err = errno;

        // Queue up remaining data
        if (sent_bytes < 0) {
            if (err == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                continue;
            }
            this->queueShutdown();
            Utils::throwIfError<true>(getLogger(), err, CHANNEL_LOG_TAG);
            break;
        }
        if (sent_bytes < bytes.size()) {
            bytes = bytes.subspan(sent_bytes);
        }
    }
}

Channel::~Channel() {
    queueShutdown();

    // Cleanup if necessary
    awaitShutdown();
}

bool Channel::isActive() const {
    return active && socket.isActive();
}

/// When the owning socket sees the active false bool, the channel will be deleted
void Channel::queueShutdown() {
    getLogger().fmtLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Queing shutdown for {}", socket.id);
    active = false;
}

void Channel::awaitShutdown() {
    queueShutdown();

    if (readThread.joinable()) {
        getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Read detach");
        readThread.join();
    }

    if (writeThread.joinable()) {
        getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Write wait end");
        writeThread.join();
    }

    // Wait for these to unlock
    std::lock(readLock, writeLock);
    readLock.unlock();
    writeLock.unlock();
//    std::lock_guard readLockGuard(readLock);
//    std::lock_guard writeLockGuard(writeLock);

    getLogger().writeLog<LoggerLevel::DEBUG_LEVEL>(CHANNEL_LOG_TAG, "Ending channel");
}


