#pragma once

#include <mutex>
#include <vector>
#include <functional>
#include <thread>
#include <optional>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdexcept>

#include "queue/blockingconcurrentqueue.h"
#include "utils/EventCallback.hpp"

#include "Message.hpp"

namespace SocketLib {

    /// Forward declares
    class SocketHandler;
    class Channel;
    class Logger;

    // int is client descriptor, true if connect, false disconnected
    using ConnectCallbackFunc = std::function<void(Channel&, bool)>;
    using ListenCallbackFunc = std::function<void(Channel&, const Message&)>;

    using ConnectEventCallback = Utils::EventCallback<Channel&, bool>;
    using ListenEventCallback = Utils::EventCallback<Channel&, const Message&>;


    class Socket {
    public:
        constexpr static const std::string_view SOCKET_LOG_TAG = "socket_core";

        virtual ~Socket(); // call through SocketHandler

        // No copying socket
        constexpr Socket(const Socket&) = delete;
        constexpr Socket& operator=(const Socket&) = delete;

        ListenEventCallback listenCallback;
        ConnectEventCallback connectCallback;

        // Friend so can call listen callbacks and disconnect
        friend class Channel;

        ///
        /// \return
        [[nodiscard]] bool isActive() const;

         /// This will set active to false and stop the threads, however it is recommended to instead
         /// delete the Server socket or at least do it after
        void notifyStop();

        // Socket settings
        uint32_t bufferSize = 512;
        bool noDelay = false; // must be set before server is bind and listening

        /// Returns true if the destructor has been called at least once
        /// \return
        [[nodiscard]] bool isDestroyed() const;

        /// The socket handler managing this socket
        /// TODO: Should we even have this or pass it manually where it's needed?
        [[nodiscard]] SocketHandler* getSocketHandler() const;
    private:
        uint32_t const id;
        friend class SocketHandler;
    protected:
        explicit Socket(SocketHandler* socketHandler, uint32_t id, std::optional<std::string> address, uint32_t port);

        SocketHandler* socketHandler;

        bool active = false;
        bool destroyed = false;

        struct addrinfo *servInfo;
        const std::optional<std::string> host;
        const uint32_t port;
        int socketDescriptor = -1;

        Logger& getLogger();

        virtual void disconnectInternal(int clientDescriptor) = 0;

    };

    class Channel {
    public:
        constexpr static const std::string_view CHANNEL_LOG_TAG = "channel";
        constexpr Channel() = delete;
        constexpr Channel(Channel const&) = delete;

        explicit Channel(Socket const& socket, Logger& logger, ListenEventCallback& listenCallback, int clientDescriptor);

        ~Channel();



        /// Will not send if message is empty or null
        /// \param msg
        void queueWrite(const Message& msg);

        std::thread readThread;
        std::thread writeThread;

        const int clientDescriptor;

        [[nodiscard]] bool isActive() const;

    private:
        bool active;

        Socket const& socket;

        // Owned by socket, which owns Channel
        Logger& logger;
        ListenEventCallback& listenCallback;

        moodycamel::BlockingConcurrentQueue<Message> writeQueue;

        Logger& getLogger();
        void sendMessage(const Message& message);
        void sendBytes(std::span<const byte> bytes);

        void awaitShutdown();
        void readThreadLoop();
        void writeThreadLoop();

        std::mutex deconstructMutex;
        moodycamel::ConsumerToken writeConsumeToken;

        void queueShutdown();
    };

}