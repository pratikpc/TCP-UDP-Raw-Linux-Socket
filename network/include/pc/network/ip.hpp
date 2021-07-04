#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

#include <unistd.h>

namespace pc
{
   namespace network
   {
      class IP
      {
       public:
         addrinfo  hints;
         addrinfo* ip;

       public:
         IP(int const socketType = SOCK_STREAM) : ip(NULL)
         {
            // See also https://stackoverflow.com/questions/9820281/confused-about-memset
            memset(&hints, 0, sizeof hints);

            hints.ai_family   = AF_UNSPEC; // AF_INET or AF_INET6 to force version
            hints.ai_socktype = socketType;
         }

         void load(std::string const& url = "", std::string const& service = "")
         {
            // IP is a linked list
            if ((getaddrinfo(!url.empty() ? url.c_str() : NULL,
                             // By default set service to NULL if empty
                             !service.empty() ? service.c_str() : NULL,
                             &hints,
                             &ip)) != 0)
            {
               throw std::runtime_error("getaddrinfo failed");
            }
         }

         void* address() const
         {
            void* addr;
            if (ip->ai_family == AF_INET)
            { // IPv4
               struct sockaddr_in* ipv4 = (struct sockaddr_in*)ip->ai_addr;
               addr                     = &(ipv4->sin_addr);
            }
            else
            { // IPv6
               struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)ip->ai_addr;
               addr                      = &(ipv6->sin6_addr);
            }
            return addr;
         }

         operator std::string() const
         {
            char ipstr[INET6_ADDRSTRLEN];
            inet_ntop(ip->ai_family, address(), ipstr, INET6_ADDRSTRLEN);
            return std::string(ipstr);
         }

         ~IP()
         {
            // This is a linked list
            // Free it on struct going out of scope
            freeaddrinfo(ip);
         }

         int socket() const
         {
            int socket = ::socket(ip->ai_family, ip->ai_socktype, ip->ai_protocol);
            if (socket == -1)
               throw std::invalid_argument("Incorrect socket");

            return socket;
         }

         int bind() const
         {
            int socketFd = socket();
            int yes;
            if (::setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
               throw std::runtime_error("Unable to set reusable");
            if (::bind(socketFd, ip->ai_addr, ip->ai_addrlen) == -1)
            {
               throw std::invalid_argument("Unable to bind to socket");
            }
            return socketFd;
         }

         int connect() const
         {
            int socketFd = socket();
            if (::connect(socketFd, ip->ai_addr, ip->ai_addrlen) == -1)
            {
               throw std::invalid_argument("Unable to connect to socket");
            }
            return socketFd;
         }

         static std::string hostName()
         {
            char hostName[100];
            if (gethostname(hostName, sizeof(hostName)) != 0)
               throw std::invalid_argument("Unexpected. Not able to store hostname");
            return std::string(hostName);
         }
      };
   } // namespace network
} // namespace pc
