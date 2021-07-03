#pragma once

#include <pc/network/Socket.hpp>
#include <pc/network/types.hpp>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>

#include <vector>

namespace pc
{
   namespace network
   {
      struct TCP : public Socket
      {
         TCP(int const socket) : Socket(socket) {}
         TCP(TCP& o) : Socket(o.socket)
         {
            o.socket = -1;
         }

         void listen(int backlog = 5)
         {
            if (::listen(socket, backlog) == -1)
            {
               throw std::runtime_error("Unable to listen");
            }
         }
         void keepAlive()
         {
            int keepcnt   = 1;
            int keepidle  = 2;
            int keepintvl = 2;
            int yes       = 1;
            flag(IPPROTO_TCP, TCP_KEEPCNT, keepcnt);
            flag(IPPROTO_TCP, TCP_KEEPIDLE, keepidle);
            flag(IPPROTO_TCP, TCP_KEEPINTVL, keepintvl);
            flag(SOL_SOCKET, SO_KEEPALIVE, yes);
         }

         void invalidate()
         {
            socket = -1;
         }

         int accept() const
         {
            int const socketFd = ::accept(socket, NULL, NULL);
            return socketFd;
         }
         void setReusable()
         {
            int yes = 1;
            flag(SOL_SOCKET, SO_REUSEADDR, yes);
         }
         static network::buffer recvRaw(int socket, std::size_t size, int flags = 0)
         {
            int opt;

            network::buffer output(size);
            if ((opt = ::recv(socket, output.data(), size, flags)) == -1)
            {
               std::cout << std::endl << "Error was : " << strerror(errno) << " : ";
               throw std::runtime_error("Unable to read data");
            }
            return output;
         }
         network::buffer recv(std::size_t size, int flags = 0)
         {
            return TCP::recvRaw(socket, size, flags);
         }
         std::size_t send(std::string const& data, int flags = 0)
         {
            return TCP::sendRaw(socket, data.data(), data.size(), flags);
         }
         static std::size_t
             sendRaw(int socket, const char* msg, size_t const len, int flags = 0)
         {
            std::size_t total = 0;
            // Send might not send all values
            while (total < len)
            {
               std::size_t const sent =
                   TCP::sendSingle(socket, msg + total, len - total, flags);
               total += sent;
            }
            return total;
         }
         static std::size_t sendRaw(int socket, std::string const& data, int flags = 0)
         {
            return sendRaw(socket, data.data(), data.size(), flags);
         }
         static std::ptrdiff_t
             sendSingle(int socket, const char* msg, size_t const len, int flags = 0)
         {
            std::ptrdiff_t const sent = ::send(socket, msg, len, flags);
            if (sent == -1)
            {
               std::cout << std::endl << "Error was : " << strerror(errno) << " : ";
               throw std::invalid_argument("Unable to send");
            }
            return sent;
         }
      };
   } // namespace network
} // namespace pc