#include "SocketHandler.hpp"
#include <iostream>

using namespace SocketLib;

ServerSocket* SocketLib::SocketHandler::createServerSocket(uint32_t port) {
    validateActive();

    std::unique_lock<std::shared_mutex> lock(socketMutex);
    uint32_t id = nextId;
    nextId = sockets.size();
    auto socket = new ServerSocket(this, id, port);

    sockets.emplace(id, socket);

    return socket;
}


SocketLib::SocketHandler::SocketHandler(int maxThreads) : active(true) {
    threadPool.reserve(maxThreads);
    for (int i = 0; i < maxThreads; i++) {
        threadPool.emplace_back(&SocketHandler::threadLoop, this);
    }
}

void SocketLib::SocketHandler::destroySocket(uint32_t id) {
    auto it = sockets.find(id);

    if (it != sockets.end()) {
        std::unique_lock<std::shared_mutex> lock(socketMutex);
        auto& socket = it->second;
        sockets.erase(it);

        // Should this even be here? Probably not
        if (!socket->isDestroyed())
            delete socket;
    }
    nextId = id;
}

void SocketLib::SocketHandler::threadLoop() {
    moodycamel::ConsumerToken consumerToken(workQueue);
    while (active) {
        WorkT work;
        // TODO: Find a way to forcefully stop waiting for deque

        if (workQueue.wait_dequeue_timed(consumerToken, work, std::chrono::milliseconds(5))) {
            try {
                work();
            } catch (std::exception& e) {
                fprintf(stderr, "Caught exception in thread pool (treat this as an error): %s", e.what());
                // TODO: Is this the best way to handle this?
            }
        } else {
            std::this_thread::yield();
        }
    }
}

void SocketLib::SocketHandler::queueWork(const WorkT& work) {
    validateActive();

    workQueue.enqueue(work);
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


    for (auto& socket : sockets) {
        delete socket.second;
    }
    sockets.clear();
}

