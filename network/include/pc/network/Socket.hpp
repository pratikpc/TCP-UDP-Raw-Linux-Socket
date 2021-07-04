#pragma once

#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace pc
{
   namespace network
   {
      class Socket
      {
       public:
         int socket;

         Socket(int socket) : socket(socket) {}
         Socket(Socket& left) : socket(left.socket)
         {
            // Leave socket in invalid state
            left.socket = -1;
         };

         bool invalid()
         {
            return socket == -1;
         }

         template <typename T>
         void flag(int level, int name, T const& val)
         {
            if (setsockopt(socket, level, name, &val, sizeof(val)) == -1)
               throw std::runtime_error("Unable to set flag");
         }

         ~Socket()
         {
            // Do not close if socket in invalid state
            if (socket != -1)
               ::close(socket);
         }
      };
   } // namespace network
} // namespace pc
