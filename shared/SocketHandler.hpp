#pragma once

#include "Socket.hpp"
#include "ServerSocket.hpp"
#include "ClientSocket.hpp"
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
        constexpr static const std::string_view SOCKET_HANDLER_LOG_TAG = "socket_handler";

        /// Creates a socket handler with the specified amount of threads in the thread pool
        /// \param maxThreads
        explicit SocketHandler(int maxThreads = 2);
        ~SocketHandler();

        // No copying socket, we only have 1 instance ever alive.
        SocketHandler(const Socket&) = delete;
        SocketHandler& operator=(SocketHandler&) = delete;

        /// Creates a server socket binding with the port specified.
        /// This does not automatically bind and start listening
        /// @tparam port
        /// @treturn The server socket pointer. You can free this with delete or use a smart pointer
        ServerSocket* createServerSocket(uint32_t port);

        /// Creates a client socket binding with the port specified.
        /// This does not automatically bind and start listening
        /// @tparam port
        /// @treturn The server socket pointer. You can free this with delete or use a smart pointer
        ClientSocket* createClientSocket(std::string const& address, uint32_t port);


        // Only way to call this properly is to call the socket's destructor
        void destroySocket(uint32_t id);
        inline void destroySocket(Socket const* s) {
            destroySocket(s->id);
        }
        inline void destroySocket(Socket const& s) {
            destroySocket(s.id);
        }

        /// Queues up work in the thread loop. Avoid calling this with code that locks
        /// \param work The work to be done in the thread loop
        void queueWork(const WorkT& work);
        void queueWork(WorkT&& work);


        /// A common instance that can be used with multiple sockets.
        /// \return
        static SocketHandler& getCommonSocketHandler();

        [[nodiscard]] SocketLib::Logger& getLogger() {
            return logger;
        }

        [[nodiscard]] SocketLib::Logger const& getLogger() const {
            return logger;
        }

    private:
        inline void validateActive() const {
            if (!active)
                throw std::runtime_error(std::string("Socket handler is no longer active!"));
        }

        SocketLib::Logger logger;

        void handleLogThread();

        uint32_t nextId = 0;
        bool active;

        /// The thread loop for the thread pool
        void threadLoop();

        std::unordered_map<uint32_t, std::unique_ptr<Socket>> sockets;
        std::shared_mutex socketMutex;

        std::vector<std::thread> threadPool;
        std::thread loggerThread;
        moodycamel::BlockingConcurrentQueue<WorkT> workQueue;
    };
}

