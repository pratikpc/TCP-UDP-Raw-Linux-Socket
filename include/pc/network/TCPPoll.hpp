#pragma once

#include <map>
#include <sys/socket.h>
#include <vector>

#include <poll.h>
#include <unistd.h>

#include <pthread.h>

#include <pc/network/MutexGuard.hpp>

namespace pc
{
   namespace network
   {
      class TCPPoll
      {
         typedef std::vector<pollfd> pollarr;

         typedef void (*Callback)(pollfd const&);
         typedef std::map<int /*socket*/, Callback> Callbacks;

         pollarr   polls;
         Callbacks callbacks;

         pthread_mutex_t pollsMutex;

         int pollRaw(std::size_t timeout)
         {
            MutexGuard lock(pollsMutex);
            return ::poll(polls.data(), polls.size(), timeout);
         }
         int poll(std::size_t timeout)
         {
            int const rv = pollRaw(timeout);
            if (rv == -1)
               throw std::runtime_error("Poll failed");
            return rv;
         }

       public:
         void PollThis(int const socket, Callback callback)
         {
            pollfd poll;
            poll.fd     = socket;
            poll.events = POLLIN;
            // Protect this during multithreaded access
            {
               MutexGuard lock(pollsMutex);
               polls.push_back(poll);
               callbacks[socket] = callback;
            }
         }

         void exec(std::size_t timeout)
         {
            int const rv = poll(timeout * 1000);
            if (rv == 0)
               // Timeout
               return;
            MutexGuard guard(pollsMutex);
            for (pollarr::iterator it = polls.begin(); it != polls.end();)
            {
               if (it->revents & POLLHUP || it->revents & POLLNVAL)
               {
                  // Erase on Holdup
                  callbacks.erase(it->fd);
                  close(it->fd);
                  // Delete current element
                  it = polls.erase(it);
                  continue;
               }
               callbacks[it->fd](*it);
               ++it;
            }
         }
      };
   } // namespace network
} // namespace pc