#pragma once
#include <memory>
#include <sys/socket.h>

namespace pc
{
   namespace network
   {
      class TCP
      {
       public:
         int socket;

         TCP(int socket) : socket(socket) {}
         TCP(TCP&& left) : socket(left.socket)
         {
            left.socket = -1;
         };

         // Copy disabled
         TCP(TCP&) = delete;

         bool invalid()
         {
            return socket == -1;
         }

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
         void* recv(std::size_t size, int flags = 0)
         {
            char* output = new char[size];
            int   opt;
            if ((opt = ::recv(socket, output, size, flags)) == -1)
            {
               std::cerr << "read() failed: " << strerror(errno) << "\n";
               throw std::runtime_error("Unable to read data");
            }
            std::cout << "Opt = " << opt << "\n";
            if (opt == 0)
               return nullptr;
            return output;
         }
         template <typename T>
         T recv(int flags = 0)
         {
            return *((T*)recv(sizeof(T), flags));
         }

         std::size_t send(const void* msg, size_t const len, int flags = 0) const
         {
            auto const sent = ::send(socket, msg, len, flags);
            if (sent == -1)
            {
               std::cerr << "send() failed: " << strerror(errno) << "\n";
               throw std::invalid_argument("Unable to send");
            }
            return sent;
         }
         template <typename T>
         std::size_t send(T& msg, int flags = 0) const
         {
            return ::send(socket, (void*)(&msg), sizeof(msg), flags);
         }

         ~TCP()
         {
            // Do not close if socket in invalid state
            if (socket != -1)
               close(socket);
         }
      };
   } // namespace network
} // namespace pc