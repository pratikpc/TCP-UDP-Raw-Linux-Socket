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
      template <bool Scale = false>
      class TCPPoll
      {
         typedef std::vector<pollfd> pollVectorFd;

         typedef void(DownCallback)(std::size_t const);
         typedef void(HealthCheckCallback)(ClientInfo&);

         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;

         pollVectorFd pollsIn;
         pollVectorFd pollsOut;
         bool         updateIssued;

         pc::threads::Mutex pollsMutex;

         ClientInfos clientInfos;

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
         DownCallback*        downCallback;
         HealthCheckCallback* healthCheckCallback;

         std::size_t balancerIndex;
         void*       callbackConfig;
         std::size_t timeout;

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

         void terminate(int socket, std::size_t indexErase)
         {
            balancer->decPriority(balancerIndex, clientInfos[socket].deadline.MaxCount());
            clientInfos.erase(socket);
            // Delete current element
            pollsIn.erase(pollsIn.begin() + indexErase);
            updateIssued = true;
         }

         void healthCheck()
         {
            std::size_t noOfDeleted = 0;

            for (pollVectorFd::iterator it = pollsOut.begin(); it != pollsOut.end(); ++it)
            {
               if (clientInfos[it->fd].deadline.HealthCheckNeeded())
               {
                  try
                  {
                     healthCheckCallback(clientInfos[it->fd]);
                  }
                  catch (std::exception& ex)
                  {
                     std::cerr << std::endl << " : " << ex.what();
                     close(it->fd);
                     std::size_t indexErase = it - pollsOut.begin() - noOfDeleted;
                     {
                        pc::threads::MutexGuard guard(pollsMutex);
                        terminate(it->fd, indexErase);
                     }
                     ++noOfDeleted;
                     // Notify user when a File Descriptor goes down
                     downCallback(balancerIndex);
                  }
               }
            }
         }

         void exec()
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
                     terminate(it->fd, indexErase);
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
                           terminate(it->fd, indexErase);
                        }
                        ++noOfDeleted;
                        // Notify user when a File Descriptor goes down
                        downCallback(balancerIndex);
                     }
                     else
                     {
                        ++clientInfos[it->fd].deadline;
                        clientInfos[it->fd].callback(
                            *it, clientInfos[it->fd], callbackConfig, *balancer, balancerIndex);
                     }
                  }
               }
               else
               {
                  ++clientInfos[it->fd].deadline;
                  clientInfos[it->fd].callback(
                      *it, clientInfos[it->fd], callbackConfig, *balancer, balancerIndex);
               }
            }
         }
      };
   } // namespace network
} // namespace pc