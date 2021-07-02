#pragma once

#include <map>
#include <tr1/unordered_map>
#include <vector>

#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/balancer/priority.hpp>

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

#include <pc/network/ClientInfo.hpp>
#include <pc/network/TCPPoll.hpp>

namespace pc
{
   namespace network
   {
      class LearnProtocol
      {
         typedef void(DownCallback)(std::size_t const);
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;

         pc::threads::Mutex pollsMutex;

         ClientInfos clientInfos;
         TCPPoll     tcpPoll;

         void terminate(int socket, std::size_t indexErase)
         {
            balancer->decPriority(balancerIndex, clientInfos[socket].deadline.MaxCount());
            clientInfos.erase(socket);
            // Delete current element
            tcpPoll -= indexErase;
         }
         void executeCallback(pollfd const& poll)
         {
            ++clientInfos[poll.fd].deadline;
            clientInfos[poll.fd].callback(
                poll, clientInfos[poll.fd], callbackConfig, *balancer, balancerIndex);
         }

         bool HealthCheckServer(pc::network::ClientInfo& clientInfo)
         {
            try
            {
               std::string message = "DOWN-CHCK";
               pc::network::TCP::sendRaw(
                   clientInfo.socket, (const char*)message.data(), message.size());
               pollfd polls[1];
               polls[0].fd     = clientInfo.socket;
               polls[0].events = POLLIN;

               if (poll(polls, 1, 10 * 1000) < 1)
                  return false;
               if (polls[0].revents & POLLIN)
               {
                  std::vector<char> data =
                      pc::network::TCP::recvRaw(clientInfo.socket, 1000);
                  if (strncmp(data.data(), "ALIVE-ALIVE", 11) != 0)
                     return false;
                  return true;
               }
            }
            catch (std::exception& ex)
            {
            }
            return false;
         }

       public:
         DownCallback* downCallback;

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
            tcpPoll += poll;
            clientInfos[socket]          = ClientInfo();
            clientInfos[socket].callback = callback;
            clientInfos[socket].socket   = socket;
            balancer->incPriority(balancerIndex, clientInfos[socket].deadline.MaxCount());
            ++clientInfos[socket].deadline;
         }

         std::size_t size() const
         {
            return tcpPoll.size();
         }

         void execHealthChecks()
         {
            for (DataQueue<pollfd>::QueueVec::iterator it = tcpPoll.dataQueue.out.begin();
                 it != tcpPoll.dataQueue.out.end();
                 ++it)
            {
               std::cout << clientInfos[it->fd].clientId << " : " << std::boolalpha
                         << clientInfos[it->fd].deadline.HealthCheckNeeded() << "\n";
               if (clientInfos[it->fd].deadline.HealthCheckNeeded())
               {
                  if (!HealthCheckServer(clientInfos[it->fd]))
                  {
                     close(it->fd);
                     std::size_t indexErase = it - tcpPoll.dataQueue.out.begin();
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

         void pollExec()
         {
            {
               pc::threads::MutexGuard lock(pollsMutex);
               tcpPoll.PerformUpdate();
            }
            int const rv = tcpPoll.poll(timeout * 1000);
            if (rv == 0)
               // Timeout
               return;
            for (DataQueue<pollfd>::QueueVec::iterator it = tcpPoll.dataQueue.out.begin();
                 it != tcpPoll.dataQueue.out.end();
                 ++it)
            {
               if (it->revents & POLLHUP || it->revents & POLLNVAL ||
                   clientInfos[it->fd].deadlineBreach())
               {
                  close(it->fd);
                  std::size_t indexErase = it - tcpPoll.dataQueue.out.begin();
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
                        std::size_t indexErase = it - tcpPoll.dataQueue.out.begin();

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