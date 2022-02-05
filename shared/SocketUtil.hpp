#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <csignal>
#include <string>
#include <sstream>
#include <fmt/format.h>

#include "SocketLogger.hpp"


namespace SocketLib {
    struct HostPort {
        std::string host;
        std::string port;
    };

    namespace Utils {
        static addrinfo* resolveEndpoint(const char* location, const char* port) {
            int status;
            struct addrinfo hints{};
            struct addrinfo *servinfo;  // will point to the results

            memset(&hints, 0, sizeof hints); // make sure the struct is empty
            hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
            hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
            hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

            if ((status = getaddrinfo(location, port, &hints, &servinfo)) != 0) {
                Logger::fmtLog<LoggerLevel::ERROR>("ResolveEndpoint", "getaddrinfo error: {}", gai_strerror(status));
                freeaddrinfo(servinfo); // free the linked-list

                return nullptr;
            }

            // servinfo now points to a linked list of 1 or more struct addrinfos

            // ... do everything until you don't need servinfo anymore ....

            return servinfo;
        }

        //// get sockaddr, IPv4 or IPv6:
        constexpr static void *get_in_addr(struct sockaddr *sa)
        {
            if (sa->sa_family == AF_INET) {
                return &(((struct sockaddr_in*)sa)->sin_addr);
            }

            return &(((struct sockaddr_in6*)sa)->sin6_addr);
        }

        /**
         * Utility for error checking with template param
         * @tparam isStatusError
         * @param err
         * @return
         */
        template<bool isStatusError>
        static const char* getProperErrorString(int err) {
            if constexpr(isStatusError) {
                return strerror(err);
            } else {
                return strerror(errno);
            }
        }

        template<LoggerLevel lvl = LoggerLevel::ERROR>
        static void logIfError(void* status, std::string_view message, std::string_view tag) {
            if (!status) {
                Logger::fmtLog<lvl>(tag, "Failed to run method because of null. At: {}", message);
            }
        }

        template<bool isStatusError = false, LoggerLevel lvl = LoggerLevel::ERROR>
        static void logIfError(int status, std::string_view tag, std::string_view message) {
            if (status != 0) {
                Logger::fmtLog<lvl>(tag, "Failed to run method: At: {} Cause: {}", message, getProperErrorString<isStatusError>(status));
            }
        }

        constexpr static void throwIfError(void* status, std::string_view tag) {
            if (!status) {
                Logger::fmtThrowError(tag, "Failed to run method because of null");
            }
        }

        template<bool isStatusError = false>
        static void throwIfError(int status, std::string_view tag, int eq = 0) {
            if (status != eq) {
                Logger::fmtThrowError(tag, "Failed to run method: {}", getProperErrorString<isStatusError>(status));
            }
        }

        static HostPort getHostByAddress(sockaddr_storage& their_addr) {
            char hostStr[NI_MAXHOST];
            char portStr[NI_MAXSERV];

            Utils::throwIfError(
                    getnameinfo((struct sockaddr *)&their_addr, sizeof their_addr,
                                hostStr, sizeof hostStr,
                                portStr, sizeof portStr,
                                NI_NUMERICHOST | NI_NUMERICSERV),
                                "getHostByAddress"
                    );

            return {
                std::string(hostStr),
                std::string(portStr)
            };
        }
    };
}