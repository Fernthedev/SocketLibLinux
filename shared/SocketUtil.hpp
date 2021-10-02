#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <csignal>
#include <string>
#include <sstream>

#ifdef DEBUG
#warning "Debug logs enabled!"
#define coutdebug std::cout
#else
#define coutdebug std::stringstream()
#endif

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
                fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
                freeaddrinfo(servinfo); // free the linked-list

                return nullptr;
            }

            // servinfo now points to a linked list of 1 or more struct addrinfos

            // ... do everything until you don't need servinfo anymore ....

            return servinfo;
        }

        inline static void throwIfError(void* status) {
            if (!status) {
                perror("Null check");
                throw std::runtime_error(std::string("Null check: "));
            }
        }

        inline static void throwIfError(int status, int eq = 0) {
            if (status != eq) {
                perror("error");
                fprintf(stderr, "error: %s\n", strerror(status));
                throw std::runtime_error(std::string("Failed to run method: ") + std::string(strerror(status)));
            }
        }

        inline static void throwIfErrorInverse(int status, int eq = 0) {
            if (status == eq) {
                perror("error");
                fprintf(stderr, "error: %s\n", strerror(status));
                throw std::runtime_error(std::string("Failed to run method: ") + std::string(strerror(status)));
            }
        }

        static HostPort getHostByAddress(sockaddr_storage& their_addr) {
            char hostStr[NI_MAXHOST];
            char portStr[NI_MAXSERV];

            Utils::throwIfError(
                    getnameinfo((struct sockaddr *)&their_addr, sizeof their_addr,
                                hostStr, sizeof hostStr,
                                portStr, sizeof portStr,
                                NI_NUMERICHOST | NI_NUMERICSERV)
                    );

            return {
                std::string(hostStr),
                std::string(portStr)
            };
        }
    };
}