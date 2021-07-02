#pragma once

#include <map>
#include <vector>

#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <pc/balancer/priority.hpp>
#include <pc/network/ClientInfo.hpp>
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

         typedef void(DownCallback)(std::size_t const);

         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;

         pollVectorFd pollsIn;
         pollVectorFd pollsOut;

         pc::threads::Mutex pollsMutex;

         ClientInfos clientInfos;

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

         std::size_t balancerIndex;
         void*       callbackConfig;

         pc::balancer::priority* balancer;

         void Add(int const socket, ClientInfo::Callback callback)
         {
            pollfd poll;
            poll.fd     = socket;
            poll.events = POLLIN;

            // Protect this during multithreaded access
            {
               pc::threads::MutexGuard lock(pollsMutex);
               pollsIn.push_back(poll);
               clientInfos[socket] = ClientInfo();
               updateIssued        = true;

               clientInfos[socket].callback = callback;
               clientInfos[socket].socket   = socket;

               balancer->incPriority(balancerIndex,
                                     clientInfos[socket].deadline.MaxCount());
               ++clientInfos[socket].deadline;
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
               if (it->revents & POLLHUP || it->revents & POLLNVAL ||
                   clientInfos[it->fd].deadlineBreach())
               {
                  close(it->fd);
                  std::size_t indexErase = it - pollsOut.begin() - noOfDeleted;
                  {
                     pc::threads::MutexGuard guard(pollsMutex);
                     balancer->decPriority(balancerIndex,
                                           clientInfos[it->fd].deadline.MaxCount());
                     clientInfos.erase(it->fd);
                     // Delete current element
                     pollsIn.erase(pollsIn.begin() + indexErase);
                     updateIssued = true;
                  }
                  ++noOfDeleted;
                  // Notify user when a File Descriptor goes down
                  downCallback(balancerIndex);
               }
               else if (it->revents & POLLIN)
               {
                  std::size_t bytes;
                  if (ioctl(it->fd, FIONREAD, &bytes) != -1)
                  {
                     if (bytes == 0)
                     {
                        close(it->fd);
                        std::size_t indexErase = it - pollsOut.begin() - noOfDeleted;
                        {
                           pc::threads::MutexGuard guard(pollsMutex);
                           balancer->decPriority(balancerIndex,
                                                 clientInfos[it->fd].deadline.MaxCount());
                           clientInfos.erase(it->fd);
                           // Delete current element
                           pollsIn.erase(pollsIn.begin() + indexErase);
                           updateIssued = true;
                        }
                        ++noOfDeleted;
                        // Notify user when a File Descriptor goes down
                        downCallback(balancerIndex);
                     }
                     else
                     {
                        ++clientInfos[it->fd].deadline;
                        clientInfos[it->fd].callback(
                            *it, clientInfos[it->fd], callbackConfig);
                     }
                  }
               }
               else
               {
                  ++clientInfos[it->fd].deadline;
                  clientInfos[it->fd].callback(*it, clientInfos[it->fd], callbackConfig);
               }
            }
         }
      };
   } // namespace network
} // namespace pc