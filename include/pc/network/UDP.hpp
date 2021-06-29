#pragma once

#include <pc/network/SimplePair.hpp>
#include <pc/network/Socket.hpp>
#include <pc/network/ip.hpp>
#include <sys/socket.h>

#include <pc/network/unique_ptr.hpp>

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

         void recv(std::size_t size, char* output, int flags = 0) const
         {
            int opt;

            sockaddr_storage their_addr;
            socklen_t        addr_len = sizeof(their_addr);

            if ((opt = ::recvfrom(
                     socket, output, size, flags, (sockaddr*)&their_addr, &addr_len)) ==
                -1)
               throw std::runtime_error("Unable to read data");
            if (opt == 0)
               output = NULL;
         }
         // template <typename T>
         // T recv(int flags = 0)
         // {
         //    return *((T*)recv(sizeof(T), flags));
         // }

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