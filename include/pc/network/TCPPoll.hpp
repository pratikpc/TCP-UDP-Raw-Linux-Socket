#pragma once

#include <sys/socket.h>

#include <mutex>
#include <vector>

#include <chrono>
#include <map>

#include <poll.h>
#include <unistd.h>

namespace pc
{
   namespace network
   {
      struct TCPPoll
      {
         typedef std::vector<pollfd> pollarr;

         typedef void (*Callback)(pollfd const&);
         typedef std::map<int /*socket*/, Callback> Callbacks;

         pollarr    polls;
         Callbacks  callbacks;
         std::mutex pollsMutex;

         int pollRaw(std::chrono::milliseconds timeout)
         {
            std::lock_guard<std::mutex> lock{pollsMutex};
            return ::poll(polls.data(), polls.size(), timeout.count());
         }
         int poll(std::chrono::milliseconds timeout)
         {
            int const rv = pollRaw(timeout);
            if (rv == -1)
               throw std::runtime_error("Poll failed");
            return rv;
         }

         void PollThis(int const socket, Callback&& callback)
         {
            pollfd poll;
            poll.fd     = socket;
            poll.events = POLLIN;
            // Protect this during multithreaded access
            std::lock_guard<std::mutex> lock{pollsMutex};
            polls.push_back(poll);
            callbacks[socket] = callback;
         }

         void exec(std::chrono::milliseconds timeout)
         {
            int const rv = poll(timeout);
            if (rv == 0)
               // Timeout
               return;
            std::lock_guard<std::mutex> guard{pollsMutex};
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