#pragma once

#include <pc/network/TCP.hpp>
#include <pc/poll/Poll.hpp>

namespace pc
{
   namespace network
   {
      namespace TCPPoll
      {
         template <typename Buffer>
         Result recvFixedBytes(::pollfd&   poll,
                               Buffer&     buffer,
                               std::size_t size,
                               std::size_t timeout)
         {
            if (timeout != 0)
            {
               int const ret = poll::single(poll, timeout);
               if (ret <= 0 || !(poll.revents & POLLIN))
               {
                  Result recvData;
                  recvData.PollFailure = true;
                  return recvData;
               }
            }
            return network::TCP::recvFixedBytes(poll.fd, buffer, size, MSG_DONTWAIT);
         }
         template <typename Buffer>
         Result
             recvFixedBytes(int fd, Buffer& buffer, std::size_t size, std::size_t timeout)
         {
            if (timeout != 0)
            {
               ::pollfd poll;
               poll.fd     = fd;
               poll.events = POLLIN;
               return recvFixedBytes(poll, buffer, size, timeout);
            }
            return network::TCP::recvFixedBytes(fd, buffer, size, MSG_DONTWAIT);
         }

         template <typename Buffer>
         Result recvAsManyAsPossible(::pollfd& poll, Buffer& buffer, std::size_t timeout)
         {
            if (timeout != 0)
            {
               int const ret = poll::single(poll, timeout);
               if (ret <= 0 || !(poll.revents & POLLIN))
               {
                  Result recvData;
                  recvData.PollFailure = true;
                  return recvData;
               }
            }
            return network::TCP::recvAsManyAsPossibleAsync(poll.fd, buffer);
         }
         template <typename Buffer>
         Result recvAsManyAsPossible(int fd, Buffer& buffer, std::size_t timeout)
         {
            if (timeout != 0)
            {
               ::pollfd poll;
               poll.fd     = fd;
               poll.events = POLLIN;
               return recvAsManyAsPossible(poll, buffer, timeout);
            }
            return network::TCP::recvAsManyAsPossibleAsync(fd, buffer);
         }

         template <typename Data>
         Result write(pollfd& poll, Data const& out, std::size_t const timeout)
         {
            if (timeout != 0)
            {
               poll.events   = POLLOUT;
               int const ret = poll::single(poll, timeout);
               if (ret <= 0 || !(poll.revents & POLLOUT))
               {
                  Result result;
                  result.PollFailure = true;
                  return result;
               }
            }
            return network::TCP::sendRaw(poll.fd, out, MSG_DONTWAIT);
         }
         template <typename Data>
         Result write(int socket, Data const& out, std::size_t timeout)
         {
            if (timeout != 0)
            {
               pollfd poll;
               poll.fd = socket;
               return TCPPoll::write(poll, out, timeout);
            }
            return network::TCP::sendRaw(socket, out, MSG_DONTWAIT);
         }
      } // namespace TCPPoll
   }    // namespace network
} // namespace pc
