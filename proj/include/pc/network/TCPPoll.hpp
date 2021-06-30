#pragma once

#include <map>
#include <vector>

#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pc/deadline.hpp>
#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

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

      template <bool Scale = false>
      class TCPPoll
      {
         typedef std::vector<pollfd> pollVectorFd;

         typedef void(DownCallback)(void*, std::size_t const);

         typedef void (*Callback)(pollfd const&);
         typedef std::tr1::unordered_map<int /*socket*/, Callback> Callbacks;
         typedef std::tr1::unordered_map<int /*Socket*/, Deadline> Deadlines;

         pollVectorFd pollsIn;
         pollVectorFd pollsOut;
         Callbacks    callbacks;

         pc::threads::Mutex pollsMutex;

         Deadlines deadlines;

         bool updateIssued;

         void pollUpdate()
         {
            pc::threads::MutexGuard lock(pollsMutex);
            if (updateIssued)
            {
               pollsOut     = pollsIn;
               updateIssued = false;
            }
         }

         int poll(std::size_t timeout)
         {
            pollUpdate();
            if (pollsOut.size() == 0)
            {
               sleep(timeout / 1000);
               return 0; // Timeout
            }
            int const rv = ::poll(pollsOut.data(), pollsOut.size(), timeout);
            if (rv == -1)
               throw std::runtime_error("Poll failed");
            return rv;
         }

       public:
         DownCallback* downCallback;

         void*       callBackParam;
         std::size_t downCallbackIndex;

         void PollThis(int const socket, Callback callback)
         {
            pollfd poll;
            poll.fd     = socket;
            poll.events = POLLIN;

            // Protect this during multithreaded access
            {
               pc::threads::MutexGuard lock(pollsMutex);
               pollsIn.push_back(poll);
               callbacks[socket] = callback;
               deadlines[socket] = pc::Deadline();
               updateIssued      = true;
            }
         }
         std::size_t size() const
         {
            return pollsIn.size();
         }
         void exec(std::size_t timeout)
         {
            int const rv = poll(timeout * 1000);
            if (rv == 0)
               // Timeout
               return;
            std::size_t noOfDeleted = 0;
            for (pollVectorFd::iterator it = pollsOut.begin(); it != pollsOut.end(); ++it)
            {
               if (it->revents & POLLHUP ||
                   it->revents & POLLNVAL
                   // Check deadline breach
                   // Disconnect if too many messages received
                   || deadlines[it->fd])
               {
                  close(it->fd);
                  std::size_t indexErase = it - pollsOut.begin() - noOfDeleted;
                  {
                     pc::threads::MutexGuard guard(pollsMutex);
                     callbacks.erase(it->fd);
                     deadlines.erase(it->fd);
                     // Delete current element
                     pollsIn.erase(pollsIn.begin() + indexErase);
                     updateIssued = true;
                  }
                  ++noOfDeleted;
                  // Notify user when a File Descriptor goes down
                  downCallback(callBackParam, downCallbackIndex);
                  continue;
               }
               if (it->revents & POLLIN)
               {
                  std::size_t bytes;
                  if (ioctl(it->fd, FIONREAD, &bytes) != -1)
                  {
                     ++deadlines[it->fd];
                     if (bytes == 0)
                     {
                        close(it->fd);
                        std::size_t indexErase = it - pollsOut.begin() - noOfDeleted;
                        {
                           pc::threads::MutexGuard guard(pollsMutex);
                           callbacks.erase(it->fd);
                           deadlines.erase(it->fd);
                           // Delete current element
                           pollsIn.erase(pollsIn.begin() + indexErase);
                           updateIssued = true;
                        }
                        ++noOfDeleted;
                        // Notify user when a File Descriptor goes down
                        downCallback(callBackParam, downCallbackIndex);
                        continue;
                     }
                  }
               }
               callbacks[it->fd](*it);
            }
         }
      };
   } // namespace network
} // namespace pc