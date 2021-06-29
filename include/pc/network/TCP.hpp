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
         std::unique_ptr<char[]> recv(std::size_t size, int flags = 0) const
         {
            auto output = std::make_unique<char[]>(size);
            int  opt;
            if ((opt = ::recv(socket, output.get(), size, flags)) == -1)
               throw std::runtime_error("Unable to read data");
            if (opt == 0)
               return nullptr;
            return output;
         }
         template <typename T>
         T recv(int flags = 0) const
         {
            return *((T*)recv(sizeof(T), flags).get());
         }

         std::size_t send(const void* msg, size_t const len, int flags = 0) const
         {
            std::size_t const sent = ::send(socket, msg, len, flags);
            if (sent == -1)
               throw std::invalid_argument("Unable to send");
            return sent;
         }
         template <typename T>
         std::size_t send(T& msg, int flags = 0) const
         {
            return send((const void*)(&msg), sizeof(msg), flags);
         }
      };
   } // namespace network
} // namespace pc