#pragma once

#include <pc/network/Socket.hpp>
#include <sys/socket.h>

#include <memory>

namespace pc
{
   namespace network
   {
      struct TCP : public Socket
      {
         using Socket::Socket;
         TCP(TCP&& o) : Socket(std::move(o)) {}

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

         TCP accept()
         {
            return TCP(::accept(socket, nullptr, nullptr));
         }
         void setReusable()
         {
            int yes;
            if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
               throw std::runtime_error("Unable to set reusable");
         }
         static std::unique_ptr<std::uint8_t[]>
             recvRaw(int socket, std::size_t size, int flags = 0)
         {
            auto output = std::make_unique<std::uint8_t[]>(size);
            int  opt;
            if ((opt = ::recv(socket, output.get(), size, flags)) == -1)
               throw std::runtime_error("Unable to read data");
            if (opt == 0)
               return nullptr;
            return output;
         }
         std::unique_ptr<std::uint8_t[]> recv(std::size_t size, int flags = 0) const
         {
            return TCP::recvRaw(socket, size, flags);
         }
         template <typename T>
         T recv(int flags = 0) const
         {
            return *((T*)recv(sizeof(T), flags).get());
         }
         std::size_t send(const std::uint8_t* msg, size_t const len, int flags = 0) const
         {
            std::size_t total = 0;
            // Send might not send all values
            while (total < len)
            {
               std::size_t const sent = sendSingle(msg + total, len - total, flags);
               total += sent;
            }
            return total;
         }

         std::size_t
             sendSingle(const std::uint8_t* msg, size_t const len, int flags = 0) const
         {
            std::size_t const sent = ::send(socket, msg, len, flags);
            if (sent == -1)
               throw std::invalid_argument("Unable to send");
            return sent;
         }
         template <typename T>
         std::size_t send(T& msg, int flags = 0) const
         {
            return send((const std::uint8_t*)(&msg), sizeof(msg), flags);
         }
      };
   } // namespace network
} // namespace pc