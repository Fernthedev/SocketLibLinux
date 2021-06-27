#pragma once

#include "Socket.hpp"
#include "ServerSocket.hpp"
#include "queue/blockingconcurrentqueue.h"

#include <mutex>
#include <shared_mutex>
#include <functional>
#include <unordered_map>
#include <queue>
#include <exception>

namespace SocketLib {
    using WorkT = std::function<void()>;

    class SocketHandler {
    public:
        explicit SocketHandler(int maxThreads = 8);
        ~SocketHandler();

        // No copying socket, we only have 1 instance ever alive.
        SocketHandler(const Socket&) = delete;
        SocketHandler& operator=(SocketHandler&) = delete;

        ServerSocket* createServerSocket(uint32_t port);

        // Only way to call this properly is to call the socket's destructor
        void destroySocket(uint32_t id);

        void queueWork(const WorkT& work);

        static SocketHandler& getCommonSocketHandler();

    private:
        inline void validateActive() const {
            if (!active)
                throw std::runtime_error(std::string("Socket handler is no longer active!"));
        }

        uint32_t nextId = 0;
        bool active;
        void threadLoop();

        std::unordered_map<uint32_t, ServerSocket*> sockets;
        std::shared_mutex socketMutex;

        std::vector<std::thread> threadPool;
        moodycamel::BlockingConcurrentQueue<WorkT> workQueue;
    };
}

