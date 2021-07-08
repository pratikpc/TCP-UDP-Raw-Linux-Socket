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

         PollFdQueue dataQueue;

       public:
         TCPPoll addSocketToPoll(int socket)
         {
            ::pollfd poll;
            poll.fd     = socket;
            poll.events = POLLIN | POLLOUT;
            return addToPoll(poll);
         }
         TCPPoll addToPoll(::pollfd const& poll)
         {
            dataQueue.Add(poll);
            return *this;
         }
         template <typename Iterable>
         TCPPoll& remove(Iterable const& socketsToRemove)
         {
            for (PollFdQueue::const_iterator it = begin(); it != end(); ++it)
            {
               int const socket = it->fd;
               // Check if the socket exists first of all
               // If it does not
               // Do nothing
               if (socketsToRemove.find(socket) == socketsToRemove.end())
               {
                  continue;
               }
               std::size_t const indexErase = it - begin();
               // Delete current element
               removeAtIndex(indexErase);
            }
            return *this;
         }
         TCPPoll& removeAtIndex(std::size_t const index)
         {
            dataQueue.RemoveAtIndex(index);
            return *this;
         }
         PollFdQueue::const_iterator begin() const
         {
            return dataQueue.begin();
         }
         PollFdQueue::iterator begin()
         {
            return dataQueue.begin();
         }
         PollFdQueue::const_iterator end() const
         {
            return dataQueue.end();
         }
         PollFdQueue::iterator end()
         {
            return dataQueue.end();
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
         template <std::size_t size>
         static int poll(pollfd (&data)[size], std::size_t timeout)
         {
            return TCPPoll::poll(data, size, timeout);
         }
         int poll(std::size_t timeout)
         {
            return TCPPoll::poll(dataQueue.data(), dataQueue.size(), timeout);
         }
         static Result readOnly(::pollfd    poll,
                                buffer&     buffer,
                                std::size_t size,
                                std::size_t timeout)
         {
            return TCPPoll::readOnly(poll.fd, buffer, size, timeout);
         }
         static Result
             readOnly(int fd, buffer& buffer, std::size_t size, std::size_t timeout)
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
         static Result read(pollfd poll, buffer& buffer, std::size_t timeout)
         {
            return TCPPoll::readOnly(poll, buffer, buffer.size(), timeout);
         }
         static Result read(int socket, buffer& buffer, std::size_t timeout)
         {
            pollfd poll;
            poll.fd = socket;
            return TCPPoll::read(poll, buffer, timeout);
         }
         static Result
             write(pollfd poll, std::string const& out, std::size_t const timeout)
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
         static Result write(int socket, std::string const& out, std::size_t timeout)
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
