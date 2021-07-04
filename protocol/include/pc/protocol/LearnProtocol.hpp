#pragma once

#include <tr1/unordered_map>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/balancer/priority.hpp>

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

#include <pc/network/TCPPoll.hpp>

#include <pc/protocol/ClientInfo.hpp>
#include <pc/protocol/Config.hpp>

#include <pc/protocol/Commands.hpp>
#include <pc/protocol/Packet.hpp>
#include <pc/protocol/types.hpp>

#include <set>

namespace pc
{
   namespace protocol
   {
      class LearnProtocol
      {
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;

         typedef std::set<int /*socket*/> HealthTerminate;

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
         NetworkSendPacket executeCallback(int socket, NetworkPacket const& readPacket)
         {
            NetworkSendPacket packet =
                clientInfos[socket].callback(readPacket, clientInfos[socket]);
            ++clientInfos[socket].deadline;
            return packet;
         }
         void executeCallback(pollfd poll)
         {
            if (!clientInfos[poll.fd].hasClientId())
            {
               return setupConnectionServer(poll, clientInfos[poll.fd]);
            }
            network::buffer buffer(100);
            NetworkPacket   readPacket = NetworkPacket::Read(poll, buffer, 0);
            if (readPacket.command == commands::downdetect::DownAlive)
            {
               clientInfos[poll.fd].scheduleTermination = false;
               return;
            }
            if (readPacket.command != commands::Send)
               return;
            NetworkSendPacket packet = executeCallback(poll.fd, readPacket);
            if (packet.command == commands::Send)
               packet.Write(poll, timeout);
         }

         void setupConnectionServer(pollfd poll, ClientInfo& clientInfo)
         {
            network::buffer data(40);
            NetworkPacket   ackAck = NetworkPacket::Read(poll, data, timeout);
            if (ackAck.command != commands::setup::Ack)
            {
               throw std::runtime_error(commands::setup::Ack +
                                        " not received. Protocol violated");
            }
            NetworkPacket const ackSyn(commands::setup::Syn);
            ackSyn.Write(poll, timeout);

            NetworkPacket const clientId = NetworkPacket::Read(poll, data, timeout);
            if (clientId.command != commands::setup::ClientID)
            {
               throw std::runtime_error(commands::setup::ClientID +
                                        " not received. Protocol violated");
            }
            clientInfo.clientId = std::string(clientId.data);

            NetworkPacket const join(commands::setup::Join);
            join.Write(poll, timeout);

            std::size_t newDeadlineMaxCount =
                config->ExtractDeadlineMaxCountFromDatabase(clientInfo.clientId);
            config->balancer->setPriority(balancerIndex,
                                          // Update priority for given element
                                          (*config->balancer)[balancerIndex] -
                                              clientInfo.deadline.MaxCount() +
                                              newDeadlineMaxCount);
            clientInfo.changeMaxCount(newDeadlineMaxCount);
         }

       public:
         std::size_t balancerIndex;
         Config*     config;
         std::size_t timeout;

         void Add(int const socket, ClientResponseCallback callback = NULL)
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
            if (config != NULL)
               config->balancer->incPriority(balancerIndex,
                                             clientInfos[socket].deadline.MaxCount());
            ++clientInfos[socket].deadline;
         }

         std::size_t size() const
         {
            return tcpPoll.size();
         }

         void execHealthChecksServer()
         {
            for (DataQueue<pollfd>::QueueVec::iterator it = tcpPoll.dataQueue.out.begin();
                 it != tcpPoll.dataQueue.out.end();
                 ++it)
               if (clientInfos[it->fd].deadline.HealthCheckNeeded())
               {
                  // If this is not the cycle to terminate
                  if (!clientInfos[it->fd].scheduleTermination)
                  {
                     // Send Down Check first
                     NetworkPacket downCheck(commands::downdetect::DownCheck);
                     downCheck.Write(*it, timeout);
                     clientInfos[it->fd].scheduleTermination = true;
                  }
                  // It is enabled
                  // So if we end up here again
                  // It means that the Health Check was not reset during
                  // So no message came
                  // Hence kill
                  else
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
         }

         pollfd clientGetFrontElem()
         {
            return tcpPoll.dataQueue.front();
         }
         NetworkPacket clientExecCallback(network::buffer& buffer)
         {
            // Will contain only one link to server
            pollfd server = tcpPoll.dataQueue.front();
            // If deadline crossed, sleep
            if (clientInfos[server.fd].deadline)
            {
               sleep(timeout / 1000);
               return NetworkPacket(commands::Empty);
            }
            NetworkPacket packet = NetworkPacket::Read(server, buffer, timeout);
            if (packet.command == commands::downdetect::DownCheck)
            {
               NetworkPacket alive(commands::downdetect::DownAlive);
               alive.Write(server, timeout);
               return clientExecCallback(buffer);
            }
            return packet;
         }
         void setupConnectionClient(std::string const clientId)
         {
            // Will contain only one link to server
            pollfd server = tcpPoll.dataQueue.front();

            NetworkPacket const ackAck(commands::setup::Ack);
            ackAck.Write(server, timeout);

            network::buffer data(40);
            NetworkPacket   ackSyn = NetworkPacket::Read(server, data, timeout);
            if (ackSyn.command != commands::setup::Syn)
            {
               throw std::runtime_error(commands::setup::Syn +
                                        " not received. Protocol violated");
            }
            NetworkPacket const clientIdSend(commands::setup::ClientID, clientId);
            clientInfos[server.fd].clientId = clientId;
            clientIdSend.Write(server, timeout);
            NetworkPacket join = NetworkPacket::Read(server, data, timeout);
            if (join.command != commands::setup::Join)
            {
               throw std::runtime_error(commands::setup::Join +
                                        " not received. Protocol violated");
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
                  char        arr[10];
                  std::size_t bytes = pc::network::TCP::recvRaw(it->fd, arr, 8, MSG_PEEK);
                  if (bytes <= 0)
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
               else
                  executeCallback(*it);
            }
         }
      };
   } // namespace protocol
} // namespace pc
