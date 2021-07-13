#pragma once

#include <pc/network/Result.hpp>
#include <pc/network/Socket.hpp>

#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>

#include <cassert>

#ifdef PC_PROFILE
#   include <cstring>
#   include <pc/timer/timer.hpp>
#endif

namespace pc
{
   namespace network
   {
      struct TCP : public Socket
      {
         TCP(int const socket) : Socket(socket) {}
         TCP(TCP& o) : Socket(o.socket)
         {
            o.invalidate();
         }

         operator pollfd() const
         {
            ::pollfd poll;
            poll.fd = socket;
            return poll;
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
         void speedUp()
         {
            disableNagel();
         }
         void disableNagel()
         {
            int yes = 1;
            flag(IPPROTO_TCP, TCP_NODELAY, yes);
         }

         static bool containsDataToRead(int socket, int const flags = 0)
         {
            char    val;
            ssize_t bytes = pc::network::TCP::recvRaw(
                socket, &val, sizeof(val), MSG_PEEK | MSG_DONTWAIT | flags);
            return bytes > 0;
         }

         static ssize_t recvRaw(int const         socket,
                                char*             input,
                                std::size_t const size,
                                int const         flags = 0)
         {
#ifndef PC_NETWORK_MOCK
            ssize_t const opt = ::recv(socket, input, size, flags);
#else
            ssize_t const opt = size;
#endif
            if (opt == -1)
               throw std::runtime_error("Unable to read data");
            return opt;
         }
         static bool HandleError(Result&      result,
                                 std::size_t& asyncFailCounter,
                                 std::size_t& majorFailCount)
         {
            int const errorCode = errno;
#ifdef PC_PROFILE
            std::cout << "HandleError " << std::strerror(errorCode) << std::endl;
#endif
            // If this is not a socket then this is a very very major error
            assert(errorCode != ENOTSOCK);
            // If the socket has not been connected or listened to
            // Major error
            assert(errorCode != ENOTCONN);
            // Bad address
            // This error cannot be recovered from
            // And may indicate bigger problems within code
            // Only check in test builds
            assert(errorCode != EFAULT);
            // Some bit in the flags argument is inappropriate for the socket type.
            assert(errorCode != EOPNOTSUPP);

            // Upon async receive failure
            if (errorCode == EAGAIN || errorCode == EWOULDBLOCK)
            {
               ++asyncFailCounter;
               if (asyncFailCounter >= 5)
               {
                  // If the async messaging fails
                  // Sleep for a few moments
                  sleep(5);
                  // Reset counter
                  asyncFailCounter = 0;
                  // Every time counter resets
                  // We consider it a major failure
                  ++majorFailCount;
               }
               return true;
            }
            // For failures that can be recovered from
            // If some time passes
            if ( // The receive was interrupted by delivery of a signal before
                 // any data was available;
                 // https://stackoverflow.com/a/41474692/1691072
                 // Recoverable
                errorCode == EINTR ||
                // No memory available
                errorCode == ENOMEM ||
                // Another Fast Open is in progress.
                errorCode == EALREADY ||
                // On timeout
                // Best to sleep and retry
                errorCode == ETIMEDOUT)
            {
               // Supports sleeping for a small while
               // And then retrying to get the
               // same data
               sleep(5);
               // We consider the occurence of this to be a major failure
               ++majorFailCount;
               return true;
            }
            if ( // If socket is invalid
                errorCode == EBADF ||
                // If local end is shut down on connection oriented socket
                // Assume this leads to socket closure
                errorCode == EPIPE ||
                // Write access was denied on the given file
                errorCode == EACCES ||
                // Connection reset by peer
                errorCode == ECONNRESET ||
                // If the other side refuses to connect
                errorCode == ECONNREFUSED ||
                // If this was not a socket descriptor in the first place
                // Note hat we have an assert running
                // But this would work better in runtime
                errorCode == ENOTSOCK ||
                // If the socket we are reading from is yet to be connected or
                // accepted This is very risky Let us just indicate closure here We
                // also have asserts running in debug builds
                errorCode == ENOTCONN ||
                // Some bit in the flags argument is inappropriate for the socket
                // type.
                errorCode == ENOTSUP)
            {
               result.SocketClosed = true;
               return false;
            }
            return false;
         }
         template <typename Buffer>
         static Result recvOnly(int const         socket,
                                Buffer&           buffer,
                                std::size_t const size,
                                int const         flags = 0)
         {
            std::size_t total            = 0;
            std::size_t asyncFailCounter = 0;
            std::size_t majorFailCounter = 0;
            Result      result;
#ifdef PC_PROFILE
            timespec start = timer::now();
#endif
            // Async recv might not recv all values
            // We are interested in
            while (total < size)
            {
#ifndef PC_NETWORK_MOCK
               ssize_t const recv =
                   ::recv(socket, buffer.data() + total, size - total, flags);
#else
               ssize_t const recv = size;
#endif
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
               if (recv == -1)
               {
                  if (!TCP::HandleError(result, asyncFailCounter, majorFailCounter))
                     break;
                  // If too many failures
                  // End loop
                  if (majorFailCounter >= 5)
                  {
                     result.SocketClosed = true;
                     break;
                  }
               }
               else
               { // We do not want to add the -1 in case AWAITABLE
                  total += recv;
               }
            }
            result.NoOfBytes = total;
#ifdef PC_PROFILE
            result.duration = timer::now() - start;
#endif
            return result;
         }
         template <typename Buffer>
         Result recvOnly(Buffer& buffer, std::size_t const size, int const flags = 0)
         {
            return TCP::recvOnly(socket, buffer, size, flags);
         }
         template <typename Buffer>
         Result recv(Buffer& buffer, int const flags = 0)
         {
            return TCP::recvOnly(socket, buffer, buffer.size(), flags);
         }
         template <typename Data>
         Result send(Data const& data, int const flags = 0)
         {
            return TCP::sendRaw(socket, data.data(), data.size(), flags);
         }
         static Result sendRaw(int const    socket,
                               const char*  msg,
                               size_t const len,
                               int const    flags = 0)
         {
            std::size_t total            = 0;
            std::size_t asyncFailCounter = 0;
            std::size_t majorFailCounter = 0;
            Result      result;

            // Send might not send all values
            while (total < len)
            {
#ifdef PC_PROFILE
               timespec start = timer::now();
#endif

#ifndef PC_NETWORK_MOCK
               ssize_t const sent = ::send(socket, msg, len, flags);
#else
               ssize_t const sent = len;
#endif
#ifdef PC_PROFILE
               result.duration = timer::now() - start;
#endif
               if (sent == -1)
               {
                  if (!TCP::HandleError(result, asyncFailCounter, majorFailCounter))
                     break;
                  // If too many failures
                  // End loop
                  if (majorFailCounter >= 5)
                  {
                     result.SocketClosed = true;
                     break;
                  }
               }
               else
                  total += sent;
            }
            result.NoOfBytes = total;
            return result;
         }
         template <typename Data>
         static Result sendRaw(int const socket, Data const& data, int const flags = 0)
         {
            return sendRaw(socket, data.data(), data.size(), flags);
         }
         static std::ptrdiff_t sendSingle(int const    socket,
                                          const char*  msg,
                                          size_t const len,
                                          int const    flags = 0)
         {
#ifndef PC_NETWORK_MOCK
            ssize_t const sent = ::send(socket, msg, len, flags);
#else
            ssize_t const sent = len;
#endif
            if (sent == -1)
               throw std::invalid_argument("Unable to send");
            return sent;
         }
      };
   } // namespace network
} // namespace pc
