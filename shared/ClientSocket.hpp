#pragma once

#include "Socket.hpp"
#include "SocketUtil.hpp"

#include <unordered_map>
#include <shared_mutex>
#include <optional>

namespace SocketLib {

    class ClientSocket : public Socket {
    public:
        constexpr static const std::string_view CLIENT_LOG_TAG = "client";

        explicit ClientSocket(SocketHandler *socketHandler, uint32_t id, std::string const& address,
                     uint32_t port);

        /// Binds to the port and starts listening
        void connect();

        ~ClientSocket() override;

        /// Queues a write to the channel
        /// \param clientDescriptor
        /// \param msg
        constexpr void write(const Message& msg) {
            channel->queueWrite(msg);
        }

        void close();

    protected:
        void writeLoop();
        void readLoop();

        void disconnectInternal(int clientDescriptor) override {
            close();
        }

    private:
        std::unique_ptr<Channel> channel;
    };
}