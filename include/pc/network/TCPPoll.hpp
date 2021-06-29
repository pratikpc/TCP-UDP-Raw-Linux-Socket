#pragma once

#include <map>
#include <sys/socket.h>
#include <vector>

#include <poll.h>
#include <unistd.h>

#include <pthread.h>

#include <pc/network/MutexGuard.hpp>
#include <pc/network/Mutex.hpp>

#include <tr1/array>
#include <tr1/unordered_map>

namespace pc
{
   namespace network
   {
      namespace
      {
         template <bool B, class T, class F>
         struct conditional
         {
            typedef T type;
         };

         template <class T, class F>
         struct conditional<false, T, F>
         {
            typedef F type;
         };
         template <bool B, class T = void>
         struct enable_if
         {
         };

         template <class T>
         struct enable_if<true, T>
         {
            typedef T type;
         };
      } // namespace

      template <bool Scale = true>
      class TCPPoll
      {
         typedef std::vector<pollfd> pollarr;

         typedef void (*Callback)(pollfd const&);
         typedef typename conditional<Scale,
                                      std::tr1::array<Callback, 100>,
                                      std::tr1::unordered_map<int /*socket*/, Callback> /* */>::type
             Callbacks;

         pollarr   polls;
         Callbacks callbacks;

         Mutex pollsMutex;

       public:
         TCPPoll() {}

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
                  // if (Scale)
                  //    callbacks.erase(it->fd);
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