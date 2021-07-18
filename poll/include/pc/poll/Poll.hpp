#pragma once

#include <cstddef>
#include <poll.h>
#include <stdexcept>
#include <unistd.h>

namespace pc
{
   namespace poll
   {
      int raw(pollfd* polls, std::size_t size, std::size_t timeout)
      {
         if (size == 0)
         {
            sleep(timeout);
            return 0; // Timeout
         }
#ifndef PC_NETWORK_MOCK
         int const rv = ::poll(polls, size, timeout * 1000);
         if (rv == -1)
            throw std::runtime_error("Poll failed");
#else
         int const rv = size;
         for (std::size_t i = 0; i < size; ++i)
            polls[i].revents = polls[i].events;
#endif
         return rv;
      }
      template <typename Polls>
      int multiple(Polls& polls, std::size_t timeout)
      {
         return poll::raw(polls.data(), polls.size(), timeout);
      }
      template <std::size_t size>
      int multiple(pollfd (&data)[size], std::size_t timeout)
      {
         return poll::raw(data, size, timeout);
      }
      int single(pollfd& poll, std::size_t timeout)
      {
         return poll::raw(&poll, 1, timeout);
      }
   } // namespace poll
} // namespace pc
