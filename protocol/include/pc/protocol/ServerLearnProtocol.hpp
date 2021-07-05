#pragma once

#include <tr1/unordered_map>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/protocol/ClientInfo.hpp>
#include <pc/protocol/Config.hpp>
#include <pc/protocol/LearnProtocol.hpp>

#include <pc/network/TCPPoll.hpp>

namespace pc
{
   namespace protocol
   {
      class ServerLearnProtocol : public LearnProtocol
      {
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;

         ClientInfos        clientInfos;
         network::TCPPoll   tcpPoll;
         pc::threads::Mutex pollsMutex;

         std::size_t size() const
         {
            return tcpPoll.size();
         }

         void terminate(DataQueue<pollfd>::QueueVec::iterator it)
         {
            close(it->fd);
            {
               pc::threads::MutexGuard guard(pollsMutex);

               std::size_t indexErase = it - tcpPoll.dataQueue.out.begin();
            config->balancer->decPriority(balancerIndex,
                                             clientInfos[it->fd].deadline.MaxCount());
               clientInfos.erase(it->fd);
            // Delete current element
            tcpPoll -= indexErase;
         }
            // Notify user when a File Descriptor goes down
            config->downCallback(balancerIndex);
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
               return setupConnection(poll, clientInfos[poll.fd]);
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

         void setupConnection(pollfd poll, ClientInfo& clientInfo)
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

         void Add(int const socket, ClientResponseCallback callback)
         {
            pollfd poll;
            poll.fd     = socket;
            poll.events = POLLIN;

            ClientInfo clientInfo = ClientInfo::createClientInfo(socket, callback);

            // Protect this during multithreaded access
            pc::threads::MutexGuard lock(pollsMutex);
            tcpPoll += poll;
            clientInfos[socket] = clientInfo;
            config->balancer->incPriority(balancerIndex,
                                          clientInfos[socket].deadline.MaxCount());
            ++clientInfos[socket].deadline;
         }

         void execHealthCheck()
         {
            if (config != NULL && !config->healthCheckDurationToPerform)
               return;

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
                     terminate(it);
               }
         }

         void poll()
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
                  return terminate(it);
               if (it->revents & POLLIN)
               {
                  char    arr[2];
                  ssize_t bytes = pc::network::TCP::recvRaw(it->fd, arr, 1, MSG_PEEK);
                  std::cout << "The number of available bytes are " << bytes << std::endl;
                  if (bytes <= 0)
                     return terminate(it);
                     executeCallback(*it);
               }
               else
                  executeCallback(*it);
            }
         }
      };
   } // namespace protocol
} // namespace pc
