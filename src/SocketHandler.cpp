#include "SocketHandler.hpp"
#include <iostream>

using namespace SocketLib;

ServerSocket *SocketLib::SocketHandler::createServerSocket(uint32_t port) {
    validateActive();

    std::unique_lock<std::shared_mutex> lock(socketMutex);
    uint32_t id = nextId;
    nextId++;
    auto socket = std::make_unique<ServerSocket>(this, id, port);

    return static_cast<ServerSocket *>(sockets.emplace(id, std::move(socket)).first->second.get());
}

ClientSocket *SocketHandler::createClientSocket(std::string const &address, uint32_t port) {
    validateActive();

    std::unique_lock<std::shared_mutex> lock(socketMutex);
    uint32_t id = nextId;
    nextId = sockets.size();
    auto socket = std::make_unique<ClientSocket>(this, id, address, port);

    return static_cast<ClientSocket *>(sockets.emplace(id, std::move(socket)).first->second.get());
}


SocketLib::SocketHandler::SocketHandler() : active(true) {
#ifndef SOCKETLIB_PAPER_LOG
    loggerThread = std::thread(&SocketHandler::handleLogThread, this);
#endif
}

void SocketLib::SocketHandler::destroySocket(uint32_t id) {
    auto it = sockets.find(id);

    if (it != sockets.end()) {
        std::unique_lock<std::shared_mutex> lock(socketMutex);
        auto &socket = it->second;
        sockets.erase(it);
        nextId = id;
    }
}

void SocketHandler::handleLogThread() {
#ifndef SOCKETLIB_PAPER_LOG
    moodycamel::ConsumerToken consumerToken(logger.logQueue);

    auto constexpr taskCount = 20;
    Logger::LogTask logTasks[taskCount];

    while (active) {
        auto dequeCount = logger.logQueue.wait_dequeue_bulk_timed(consumerToken, logTasks, taskCount, std::chrono::milliseconds(100));
        if (dequeCount == 0) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        for (size_t i = 0; i < dequeCount; i++) {
            auto const& task = logTasks[i];
            logger.loggerCallback.invoke(task.level, task.tag, task.log);
        }
    }
#endif
}



SocketLib::SocketHandler &SocketLib::SocketHandler::getCommonSocketHandler() {
    static SocketHandler socketHandler;

    return socketHandler;
}

SocketHandler::~SocketHandler() {
    active = false;
    std::unique_lock<std::shared_mutex> lock(socketMutex);

    if (loggerThread.joinable()) {
        loggerThread.detach();
    }
    sockets.clear();
}

