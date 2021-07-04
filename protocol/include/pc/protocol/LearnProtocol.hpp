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

#include <pc/network/TCPPoll.hpp>

#include <pc/protocol/ClientInfo.hpp>
#include <pc/protocol/Config.hpp>

namespace pc
{
   namespace protocol
   {
      class LearnProtocol
      {
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;

         pc::threads::Mutex pollsMutex;

         ClientInfos      clientInfos;
         network::TCPPoll tcpPoll;

         void terminate(int socket, std::size_t indexErase)
         {
            config->balancer->decPriority(balancerIndex,
                                          clientInfos[socket].deadline.MaxCount());
            clientInfos.erase(socket);
            // Delete current element
            tcpPoll -= indexErase;
         }
         void executeCallback(pollfd const& poll)
         {
            if (!clientInfos[poll.fd].hasClientId())
               SetupConnection(poll, clientInfos[poll.fd]);
            else
            {
               clientInfos[poll.fd].callback(poll, clientInfos[poll.fd]);
               ++clientInfos[poll.fd].deadline;
            }
         }

         bool HealthCheckServer(ClientInfo& clientInfo)
         {
            try
            {
               // Send Down Check first
               pc::network::TCP::sendRaw(clientInfo.socket, "DOWN-CHCK");
               // The client must respond with alive
               network::buffer data(11);
               tcpPoll.read(clientInfo.socket, data, timeout);
               if (!data || strncmp(data, "ALIVE-ALIVE", 11) != 0)
                  return false;
               return true;
            }
            catch (std::exception& ex)
            {
#ifdef DEBUG
               std::cout << "\nHealthCheckServer\t:Exception thrown\t:" << ex.what();
#endif
            }
            return false;
         }

       public:
         std::size_t balancerIndex;
         Config*     config;
         std::size_t timeout;

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
            config->balancer->incPriority(balancerIndex,
                                          clientInfos[socket].deadline.MaxCount());
            ++clientInfos[socket].deadline;
         }

         std::size_t size() const
         {
            return tcpPoll.size();
         }

         void read(pollfd& poll, network::buffer& buffer)
         {
            return network::TCPPoll::read(poll, buffer, timeout);
         }
         void readOnly(pollfd& poll, network::buffer& buffer, std::size_t size)
         {
            return network::TCPPoll::readOnly(poll, buffer, size, timeout);
         }
         void SetupConnection(pollfd poll, ClientInfo& clientInfo)
         {
            network::buffer data(40);
            readOnly(poll, data, 7);
            if (!data || strncmp(data, "ACK-ACK", 7) != 0)
            {
               std::cout << "ACK-ACK not received\n";
               throw std::runtime_error("ACK-ACK not received. Protocol violated");
            }
            pc::network::TCP::sendRaw(poll.fd, "ACK-SYN");
               std::cout << "ACK-SYN send" << data->data() << "\n";

            read(poll, data);
            clientInfo.clientId = std::string(data);
               std::cout << "Cklient-ID received" << data->data() << "\n";

            pc::network::TCP::sendRaw(poll.fd, "JOIN");
            std::size_t newDeadlineMaxCount =
                config->ExtractDeadlineMaxCountFromDatabase(clientInfo.clientId);
            config->balancer->setPriority(balancerIndex,
                                          // Update priority for given element
                                          (*config->balancer)[balancerIndex] -
                                              clientInfo.deadline.MaxCount() +
                                              newDeadlineMaxCount);
            clientInfo.changeMaxCount(newDeadlineMaxCount);
         }

         void execHealthChecks()
         {
            for (DataQueue<pollfd>::QueueVec::iterator it = tcpPoll.dataQueue.out.begin();
                 it != tcpPoll.dataQueue.out.end();
                 ++it)
               if (clientInfos[it->fd].deadline.HealthCheckNeeded())
                  if (!HealthCheckServer(clientInfos[it->fd]))
                  {
                     close(it->fd);
                     std::size_t indexErase = it - tcpPoll.dataQueue.out.begin();
                     {
                        pc::threads::MutexGuard guard(pollsMutex);
                        terminate(it->fd, indexErase);
                     }
                     // Notify user when a File Descriptor goes down
                     config->downCallback(balancerIndex);
                  }
         }

         void pollExec()
         {
            {
               pc::threads::MutexGuard lock(pollsMutex);
               tcpPoll.PerformUpdate();
            }
            if (tcpPoll.poll(timeout) == 0)
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
                  config->downCallback(balancerIndex);
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
                        {
                           pc::threads::MutexGuard guard(pollsMutex);
                           terminate(it->fd, indexErase);
                        }
                        // Notify user when a File Descriptor goes down
                        config->downCallback(balancerIndex);
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
   } // namespace protocol
} // namespace pc
