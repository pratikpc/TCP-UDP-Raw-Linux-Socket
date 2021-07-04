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
         static ssize_t recvRaw(int const         socket,
                                char*             input,
                                std::size_t const size,
                                int const         flags = 0)
         {
            ssize_t opt = ::recv(socket, input, size, flags);
            if (opt == -1)
            {
               std::cout << std::endl << "Error was : " << strerror(errno) << " : ";
               throw std::runtime_error("Unable to read data");
            }
            return opt;
         }
         static void recvOnly(int const         socket,
                              network::buffer&  buffer,
                              std::size_t const size,
                              int const         flags = 0)
         {
            std::size_t total = 0;
            // Async recv might not recv all values
            // We are interested in
            while (total < size)
            {
               ssize_t const recv =
                   ::recv(socket, buffer->data() + total, size - total, flags);
               if (recv == 0)
                  break;
               // If this is not Async
               // End loop or this will have an endless wait
               // Because sync recv
               // Waits till data available
               if (!(flags & MSG_DONTWAIT))
               {
                  // As we are ending the loop
                  // As recv to the current count of data
                  // To ensure we don't set Buffer to empty
                  total += recv;
                  break;
               }
               // As this is an awaitable loop
               // If recv is -1
               // It means no available data
               // Break
               if (recv == -1)
                  break;
               // We do not want to add the -1 in case AWAITABLE
               total += recv;
            }
            if (total == 0)
               // If empty
               buffer = false;
            else
               buffer = true;
         }
         static void recv(int const socket, network::buffer& buffer, int const flags = 0)
         {
            return TCP::recvOnly(socket, buffer, buffer, flags);
         }
         void recvOnly(network::buffer&  buffer,
                       std::size_t const size,
                       int const         flags = 0)
         {
            return TCP::recvOnly(socket, buffer, size, flags);
         }
         void recv(network::buffer& buffer, int const flags = 0)
         {
            return TCP::recv(socket, buffer, flags);
         }
         std::size_t send(std::string const& data, int const flags = 0)
         {
            return TCP::sendRaw(socket, data.data(), data.size(), flags);
         }
         static std::size_t sendRaw(int const    socket,
                                    const char*  msg,
                                    size_t const len,
                                    int const    flags = 0)
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
         static std::size_t
             sendRaw(int const socket, std::string const& data, int const flags = 0)
         {
            return sendRaw(socket, data.data(), data.size(), flags);
         }
         static std::ptrdiff_t sendSingle(int const    socket,
                                          const char*  msg,
                                          size_t const len,
                                          int const    flags = 0)
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
