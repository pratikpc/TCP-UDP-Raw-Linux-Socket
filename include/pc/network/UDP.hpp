#pragma once

#include <pc/network/SimplePair.hpp>
#include <pc/network/Socket.hpp>
#include <pc/network/ip.hpp>
#include <sys/socket.h>

#include <memory>

namespace pc
{
   namespace network
   {
      struct UDP : public Socket
      {
         using Socket::Socket;
         UDP(UDP&& o) : Socket(std::move(o)) {}

         SimplePair<std::unique_ptr<char[]>, sockaddr_storage> recv(std::size_t size,
                                                                    int flags = 0) const
         {
            auto output = std::make_unique<char[]>(size);

            int opt;

            sockaddr_storage their_addr;
            socklen_t        addr_len = sizeof(their_addr);

            if ((opt = ::recvfrom(socket,
                                  output.get(),
                                  size,
                                  flags,
                                  (sockaddr*)&their_addr,
                                  &addr_len)) == -1)
               throw std::runtime_error("Unable to read data");
            if (opt == 0)
               return {nullptr, {}};
            return {std::move(output), their_addr};
         }
         template <typename T>
         T recv(int flags = 0) const
         {
            return *((T*)recv(sizeof(T), flags).first);
         }

         std::size_t
             send(const void* msg, size_t const len, IP const& ip, int flags = 0) const
         {
            std::size_t const sent =
                ::sendto(socket, msg, len, flags, ip.ip->ai_addr, ip.ip->ai_addrlen);
            if (sent == -1)
               throw std::invalid_argument("Unable to send");
            return sent;
         }
         template <typename T>
         std::size_t send(T& msg, IP const& ip, int flags = 0) const
         {
            return send((const void*)(&msg), sizeof(msg), ip, flags);
         }
      };
   } // namespace network
} // namespace pc