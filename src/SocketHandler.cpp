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


SocketLib::SocketHandler::SocketHandler(int maxThreads) : active(true) {
    threadPool.reserve(maxThreads);
    for (int i = 0; i < maxThreads; i++) {
        threadPool.emplace_back(&SocketHandler::threadLoop, this);
    }

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

void SocketLib::SocketHandler::threadLoop() {
    moodycamel::ConsumerToken consumerToken(workQueue);
    auto const logToken = logger.createProducerToken();
    WorkT works[SOCKET_LIB_MAX_QUEUE_SIZE];

    while (active) {
        // TODO: Find a way to forcefully stop waiting for deque

        auto dequeCount = workQueue.wait_dequeue_bulk_timed(consumerToken, works, SOCKET_LIB_MAX_QUEUE_SIZE,
                                                            std::chrono::milliseconds(300));

        if (dequeCount == 0) {
            std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        for (size_t i = 0; i < dequeCount; i++) {
            auto const &work = works[i];
            if (!work) continue;
            try {
                work();
            } catch (std::exception &e) {
                logger.fmtLog<LoggerLevel::ERROR>(logToken, SOCKET_HANDLER_LOG_TAG,
                                                  "Caught exception in thread pool (treat this as an error): {}",
                                                  e.what());
                // TODO: Is this the best way to handle this?
            }
        }
    }
}

void SocketLib::SocketHandler::queueWork(const WorkT &work) {
    if (!work) return;
    validateActive();

    workQueue.enqueue(work);
}

void SocketLib::SocketHandler::queueWork(WorkT &&work) {
    if (!work) {
        return;
    }
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
    for (auto &thread: threadPool) {
        if (!thread.joinable()) {
            continue;
        }

        thread.join();
    }

    threadPool.clear();
    if (loggerThread.joinable()) {
        loggerThread.detach();
    }
    sockets.clear();
}

