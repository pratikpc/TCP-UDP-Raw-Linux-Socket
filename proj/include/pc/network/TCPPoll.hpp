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

#include <pc/dataQueue.hpp>

namespace pc
{
   namespace network
   {
      class TCPPoll
      {
         typedef void(DownCallback)(std::size_t const);
         typedef void(HealthCheckCallback)(ClientInfo&);

         typedef pc::DataQueue<pollfd>                               PollFdQueue;
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;

         pc::threads::Mutex pollsMutex;
         ClientInfos        clientInfos;
         PollFdQueue        dataQueue;

         void terminate(int socket, std::size_t indexErase)
         {
            balancer->decPriority(balancerIndex, clientInfos[socket].deadline.MaxCount());
            clientInfos.erase(socket);
            // Delete current element
            dataQueue -= indexErase;
         }
         void executeCallback(pollfd const& poll)
         {
            ++clientInfos[poll.fd].deadline;
            clientInfos[poll.fd].callback(
                poll, clientInfos[poll.fd], callbackConfig, *balancer, balancerIndex);
         }
         int poll(std::size_t timeout)
         {
            if (size() == 0)
            {
               sleep(timeout / 1000);
               return 0; // Timeout
            }
            int const rv = ::poll(dataQueue.data(), dataQueue.size(), timeout);
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
               pc::threads::MutexGuard lock(pollsMutex);
            dataQueue += poll;
            clientInfos[socket]          = ClientInfo();
               clientInfos[socket].callback = callback;
               clientInfos[socket].socket   = socket;
            balancer->incPriority(balancerIndex, clientInfos[socket].deadline.MaxCount());
               ++clientInfos[socket].deadline;
            }
         
         std::size_t size() const
         {
            return dataQueue.size();
         }

         void healthCheck()
         {
            for (DataQueue<pollfd>::QueueVec::iterator it = dataQueue.out.begin();
                 it != dataQueue.out.end();
                 ++it)
            {
               if (clientInfos[it->fd].deadline.HealthCheckNeeded())
               {
                  try
                  {
                     healthCheckCallback(clientInfos[it->fd]);
                  }
                  catch (std::exception& ex)
                  {
                     close(it->fd);
                     std::size_t indexErase = it - dataQueue.out.begin();
                     {
                        pc::threads::MutexGuard guard(pollsMutex);
                        terminate(it->fd, indexErase);
                     }
                     // Notify user when a File Descriptor goes down
                     downCallback(balancerIndex);
                  }
               }
            }
         }

         void exec()
         {
            {
               pc::threads::MutexGuard lock(pollsMutex);
               dataQueue.PerformUpdate();
            }
            int const rv = poll(timeout * 1000);
            if (rv == 0)
               // Timeout
               return;
            for (DataQueue<pollfd>::QueueVec::iterator it = dataQueue.out.begin();
                 it != dataQueue.out.end();
                 ++it)
            {
               if (it->revents & POLLHUP || it->revents & POLLNVAL ||
                   clientInfos[it->fd].deadlineBreach())
               {
                  close(it->fd);
                  std::size_t indexErase = it - dataQueue.out.begin();
                  {
                     pc::threads::MutexGuard guard(pollsMutex);
                     terminate(it->fd, indexErase);
                  }
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
                        std::size_t indexErase = it - dataQueue.out.begin();

                           pc::threads::MutexGuard guard(pollsMutex);
                           terminate(it->fd, indexErase);
                        // Notify user when a File Descriptor goes down
                        downCallback(balancerIndex);
                     }
                     else
                        executeCallback(*it);
                  }
               }
               else
                  executeCallback(*it);
            }
         }
      };
   } // namespace network
} // namespace pc