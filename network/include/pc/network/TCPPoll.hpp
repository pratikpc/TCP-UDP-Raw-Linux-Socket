#pragma once

#include <pc/dataQueue.hpp>

#include <poll.h>

#include <pc/network/TCP.hpp>
#include <pc/network/types.hpp>

namespace pc
{
   namespace network
   {
      class TCPPoll
      {
       public:
         typedef pc::DataQueue<pollfd> PollFdQueue;
         PollFdQueue                   dataQueue;

       public:
         TCPPoll& operator+=(pollfd const& val)
         {
            dataQueue += val;
            return *this;
         }
         TCPPoll& operator-=(std::size_t index)
         {
            dataQueue -= index;
            return *this;
         }

         static int poll(pollfd* polls, std::size_t size, std::size_t timeout)
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
         static int poll(pollfd& poll, std::size_t timeout)
         {
            return TCPPoll::poll(&poll, 1, timeout);
         }
         static int poll(PollFdQueue& data, std::size_t timeout)
         {
            return TCPPoll::poll(data.data(), data.size(), timeout);
         }
         template <std::size_t size>
         static int poll(pollfd (&data)[size], std::size_t timeout)
         {
            return TCPPoll::poll(data, size, timeout);
         }
         int poll(std::size_t timeout)
         {
            return TCPPoll::poll(dataQueue, timeout);
         }

         static std::size_t
             readOnly(pollfd poll, buffer& buffer, std::size_t size, std::size_t timeout)
         {
            poll.events   = POLLIN;
            int const ret = TCPPoll::poll(poll, timeout);
            if (ret == 0 || !(poll.revents & POLLIN))
            {
               buffer = false;
               return 0;
            }
            return network::TCP::recvOnly(poll.fd, buffer, size, MSG_DONTWAIT);
         }
         static std::size_t read(pollfd poll, buffer& buffer, std::size_t timeout)
         {
            return TCPPoll::readOnly(poll, buffer, buffer, timeout);
         }
         static std::size_t read(int socket, buffer& buffer, std::size_t timeout)
         {
            pollfd poll;
            poll.fd = socket;
            return TCPPoll::read(poll, buffer, timeout);
         }
         static std::size_t
             write(pollfd poll, std::string const& out, std::size_t timeout)
         {
            poll.events   = POLLOUT;
            int const ret = TCPPoll::poll(poll, timeout);
            if (ret == 0 || !(poll.revents & POLLOUT))
            {
               return 0;
            }
            return network::TCP::sendRaw(poll.fd, out, MSG_DONTWAIT);
         }
         static std::size_t write(int socket, std::string const& out, std::size_t timeout)
         {
            pollfd poll;
            poll.fd = socket;
            return TCPPoll::write(poll, out, timeout);
         }

         std::size_t size() const
         {
            return dataQueue.size();
         }

         void PerformUpdate()
         {
            return dataQueue.PerformUpdate();
         }
      };
   } // namespace network
} // namespace pc
