#pragma once

#include <pc/network/Socket.hpp>
#include <sys/socket.h>

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
            int yes;
            if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
               throw std::runtime_error("Unable to set reusable");
         }
         static std::vector<char> recvRaw(int socket, std::size_t size, int flags = 0)
         {
            int opt;

            std::vector<char> output(size);
            if ((opt = ::recv(socket, output.data(), size, flags)) == -1)
               throw std::runtime_error("Unable to read data");
            return output;
         }
         std::vector<char> recv(std::size_t size, int flags = 0)
         {
            return TCP::recvRaw(socket, size, flags);
         }
         // template <typename T>
         // T recv(int flags = 0) const
         // {
         //    return *((T*)recv(sizeof(T), flags));
         // }
         std::size_t send(const char* msg, size_t const len, int flags = 0) const
         {
            return TCP::sendRaw(socket, msg, len, flags);
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

         static std::ptrdiff_t
             sendSingle(int socket, const char* msg, size_t const len, int flags = 0)
         {
            std::ptrdiff_t const sent = ::send(socket, msg, len, flags);
            if (sent == -1)
               throw std::invalid_argument("Unable to send");
            return sent;
         }
         template <typename T>
         std::ptrdiff_t send(T& msg, int flags = 0) const
         {
            return send((const char*)(&msg), sizeof(msg), flags);
         }
      };
   } // namespace network
} // namespace pc