#include "SocketHandler.hpp"
#include <iostream>

using namespace SocketLib;

ServerSocket* SocketLib::SocketHandler::createServerSocket(uint32_t port) {
    validateActive();

    std::unique_lock<std::shared_mutex> lock(socketMutex);
    uint32_t id = nextId;
    nextId = sockets.size();
    auto socket = std::make_unique<ServerSocket>(this, id, port);

    return static_cast<ServerSocket *>(sockets.emplace(id, std::move(socket)).first->second.get());
}

ClientSocket *SocketHandler::createClientSocket(std::string const& address, uint32_t port) {
    validateActive();

    std::unique_lock<std::shared_mutex> lock(socketMutex);
    uint32_t id = nextId;
    nextId = sockets.size();
    auto socket = std::make_unique<ClientSocket>(this, id, address, port);

    return static_cast<ClientSocket *>(sockets.emplace(id, std::move(socket)).first->second.get());
}


SocketLib::SocketHandler::SocketHandler(int maxThreads) : active(true) {
    threadPool.reserve(maxThreads);
    for (int i = 0; i < maxThreads; i++) {
        threadPool.emplace_back(&SocketHandler::threadLoop, this);
    }

    loggerThread = std::thread(&SocketHandler::handleLogThread, this);
}

void SocketLib::SocketHandler::destroySocket(uint32_t id) {
    auto it = sockets.find(id);

    if (it != sockets.end()) {
        std::unique_lock<std::shared_mutex> lock(socketMutex);
        auto& socket = it->second;
        sockets.erase(it);
        nextId = id;
    }
}

void SocketHandler::handleLogThread() {
#ifndef SOCKETLIB_PAPER_LOG
    moodycamel::ConsumerToken consumerToken(logger.logQueue);

    while (active) {
        Logger::LogTask task;
        if (!logger.logQueue.try_dequeue(consumerToken, task)) {
            std::this_thread::yield();
            continue;
        }

        logger.loggerCallback.invoke(task.level, task.tag, task.log);
    }
#endif
}

void SocketLib::SocketHandler::threadLoop() {
    moodycamel::ConsumerToken consumerToken(workQueue);
    auto logToken = logger.createProducerToken();
    while (active) {
        WorkT work;
        // TODO: Find a way to forcefully stop waiting for deque

        if (!workQueue.try_dequeue(consumerToken, work)) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }
        try {
            work();
        } catch (std::exception &e) {
            logger.fmtLog<LoggerLevel::DEBUG_LEVEL>(logToken, SOCKET_HANDLER_LOG_TAG,
                                                    "Caught exception in thread pool (treat this as an error): {}",
                                                    e.what());
            // TODO: Is this the best way to handle this?
        }
    }
}

void SocketLib::SocketHandler::queueWork(const WorkT& work) {
    validateActive();

    workQueue.enqueue(work);
}

void SocketLib::SocketHandler::queueWork(WorkT&& work) {
    validateActive();

    workQueue.enqueue(std::move(work));
}

SocketLib::SocketHandler &SocketLib::SocketHandler::getCommonSocketHandler() {
    static SocketHandler socketHandler;

    return socketHandler;
}

SocketHandler::~SocketHandler() {
    active = false;
    std::unique_lock<std::shared_mutex> lock(socketMutex);
    for (auto& threads : threadPool) {
        threads.join();
    }

    threadPool.clear();
    loggerThread.detach();
    sockets.clear();
}

