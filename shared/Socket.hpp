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

    // int is client descriptor, true if connect, false disconnected
    using ConnectCallbackFunc = std::function<void(Channel&, bool)>;
    using ListenCallbackFunc = std::function<void(Channel&, const Message&)>;

    using ConnectEventCallback = Utils::EventCallback<Channel&, bool>;
    using ListenEventCallback = Utils::EventCallback<Channel&, const Message&>;


    class Socket {
    public:
        virtual ~Socket();

        // No copying socket
        constexpr Socket(const Socket&) = delete;
        constexpr Socket& operator=(const Socket&) = delete;

        ListenEventCallback listenCallback;
        ConnectEventCallback connectCallback;

        ///
        /// \return
        [[nodiscard]] bool isActive() const;

         /// This will set active to false and stop the threads, however it is recommended to instead
         /// delete the Server socket or at least do it after
        void notifyStop();

        // Socket settings
        uint32_t bufferSize = 512;

        /// Returns true if the destructor has been called at least once
        /// \return
        [[nodiscard]] bool isDestroyed() const;

        /// The socket handler managing this socket
        /// TODO: Should we even have this or pass it manually where it's needed?
        [[nodiscard]] SocketHandler* getSocketHandler() const;
    private:
        uint32_t id;
    protected:
        explicit Socket(SocketHandler* socketHandler, uint32_t id, std::optional<std::string> address, uint32_t port);

        SocketHandler* socketHandler;

        bool active = false;
        bool destroyed = false;

        struct addrinfo *servInfo;
        const std::optional<std::string> host;
        const uint32_t port;
        int socketDescriptor = -1;

        virtual void onConnectedClient(int clientDescriptor) = 0;
        virtual void onDisconnectClient(int clientDescriptor) = 0;



    };

    class Channel {
    public:
        constexpr Channel() = delete;

        explicit Channel(Socket& socket, int clientDescriptor, std::function<void(int)> onDisconnect, ListenEventCallback& listenCallbacks);

        ~Channel();

        void readThreadLoop();
        void writeThreadLoop();

        void queueWrite(const Message& msg);

        std::thread readThread;
        std::thread writeThread;

        const int clientDescriptor;

    private:
        const std::function<void(int)> onDisconnectEvent;

        // reference to socket
        ListenEventCallback& listenCallbacks;
        bool active;

        /// TODO: Should we add a getter for Socket? Should it be const?
        Socket& socket;

        moodycamel::BlockingConcurrentQueue<Message> writeQueue;

        void sendData(const Message& message);

        std::mutex deconstructMutex;
        moodycamel::ConsumerToken writeConsumeToken;
    };

}