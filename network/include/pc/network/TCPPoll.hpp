#pragma once

#include <pc/dataQueue.hpp>

#include <poll.h>

#include <pc/network/TCP.hpp>
#include <pc/network/types.hpp>

namespace pc
{
   namespace network
   {
      namespace TCPPoll
      {

         int poll(pollfd* polls, std::size_t size, std::size_t timeout)
         {
            if (size == 0)
            {
               sleep(timeout);
               return 0; // Timeout
            }
            int const rv = ::poll(polls, size, timeout * 1000);
            if (rv == -1)
               throw std::runtime_error("Poll failed");
            return rv;
         }
         int poll(std::vector<pollfd>& polls, std::size_t timeout)
         {
            return TCPPoll::poll(polls.data(), polls.size(), timeout);
         }
         int poll(pollfd& poll, std::size_t timeout)
         {
            return TCPPoll::poll(&poll, 1, timeout);
         }
         template <std::size_t size>
         int poll(pollfd (&data)[size], std::size_t timeout)
         {
            return TCPPoll::poll(data, size, timeout);
         }

         Result readOnly(int fd, buffer& buffer, std::size_t size, std::size_t timeout)
         {
            ::pollfd poll;
            poll.fd     = fd;
            poll.events = POLLIN;

            int const ret = TCPPoll::poll(poll, timeout);
            if (ret == 0 || !(poll.revents & POLLIN))
            {
               Result recvData;
               recvData.PollFailure = true;
               return recvData;
            }
            return network::TCP::recvOnly(poll.fd, buffer, size, MSG_DONTWAIT);
         }
         Result readOnly(::pollfd    poll,
                         buffer&     buffer,
                         std::size_t size,
                         std::size_t timeout)
         {
            return TCPPoll::readOnly(poll.fd, buffer, size, timeout);
         }
         Result read(pollfd poll, buffer& buffer, std::size_t timeout)
         {
            return TCPPoll::readOnly(poll, buffer, buffer.size(), timeout);
         }
         Result read(int socket, buffer& buffer, std::size_t timeout)
         {
            pollfd poll;
            poll.fd = socket;
            return TCPPoll::read(poll, buffer, timeout);
         }
         Result write(pollfd poll, std::string const& out, std::size_t const timeout)
         {
            poll.events   = POLLOUT;
            int const ret = TCPPoll::poll(poll, timeout);
            if (ret == 0 || !(poll.revents & POLLOUT))
            {
               Result result;
               result.PollFailure = true;
               return result;
            }
            return network::TCP::sendRaw(poll.fd, out, MSG_DONTWAIT);
         }
         Result write(int socket, std::string const& out, std::size_t timeout)
         {
            pollfd poll;
            poll.fd = socket;
            return TCPPoll::write(poll, out, timeout);
         }
      } // namespace TCPPoll
   }    // namespace network
} // namespace pc
