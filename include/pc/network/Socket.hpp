#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>

namespace pc
{
   namespace network
   {
      class Socket
      {
       public:
         int socket;

         Socket(int socket) : socket(socket) {}
         Socket(Socket&& left) : socket(left.socket)
         {
            // Leave socket in invalid state
            left.socket = -1;
         };

         // Copy disabled
         Socket(Socket&) = delete;

         bool invalid()
         {
            return socket == -1;
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