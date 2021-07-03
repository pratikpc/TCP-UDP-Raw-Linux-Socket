#pragma once

#include <pc/network/Socket.hpp>
#include <pc/network/types.hpp>
#include <pc/network/ip.hpp>
#include <sys/socket.h>

#include <vector>

namespace pc
{
   namespace network
   {
      struct UDP : public Socket
      {
         UDP(int socket) : Socket(socket) {}

         UDP(UDP& o) : Socket(o.socket)
         {
            o.socket = -1;
         }

         network::buffer recv(std::size_t size, int flags = 0) const
         {
            int opt;

            sockaddr_storage  their_addr;
            socklen_t         addr_len = sizeof(their_addr);
            network::buffer output(size);
            if ((opt = ::recvfrom(socket,
                                  output.data(),
                                  size,
                                  flags,
                                  (sockaddr*)&their_addr,
                                  &addr_len)) == -1)
               throw std::runtime_error("Unable to read data");
            if (opt == 0)
               return network::buffer();
            return output;
         }
         // template <typename T>
         // T recv(int flags = 0)
         // {
         //    return *((T*)recv(sizeof(T), flags));
         // }

         std::ptrdiff_t
             send(const void* msg, size_t const len, IP const& ip, int flags = 0) const
         {
            std::ptrdiff_t const sent =
                ::sendto(socket, msg, len, flags, ip.ip->ai_addr, ip.ip->ai_addrlen);
            if (sent == -1)
               throw std::invalid_argument("Unable to send");
            return sent;
         }
         template <typename T>
         std::ptrdiff_t send(T& msg, IP const& ip, int flags = 0) const
         {
            return send((const void*)(&msg), sizeof(msg), ip, flags);
         }
      };
   } // namespace network
} // namespace pc