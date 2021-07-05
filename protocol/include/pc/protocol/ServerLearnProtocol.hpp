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
         typedef DataQueue<pollfd>::QueueVec::iterator               QueueIterator;

         ClientInfos        clientInfos;
         network::TCPPoll   tcpPoll;
         pc::threads::Mutex pollsMutex;

         std::size_t size() const
         {
            return tcpPoll.size();
         }

         void closeConnection(QueueIterator it)
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
         void executeCallback(QueueIterator it)
         {
            if (!clientInfos[it->fd].hasClientId())
            {
               return setupConnection(it, clientInfos[it->fd]);
            }
            network::buffer buffer(UINT16_MAX);
            NetworkPacket   readPacket = NetworkPacket::Read(*it, buffer, 0);
            if (readPacket.command == Commands::DownDetect::DownAlive)
            {
               clientInfos[it->fd].scheduleTermination = false;
               return;
            }
            if (readPacket.command == Commands::MajorErrors::SocketClosed)
               return closeConnection(it);
            if (readPacket.command != Commands::Send)
               return;
            clientInfos[it->fd].scheduleTermination = false;
            NetworkSendPacket const packet          = executeCallback(it->fd, readPacket);
            if (packet.command == Commands::Send)
               if(!WritePacket(packet, it))
                  closeConnection(it);
         }

         bool WritePacket(NetworkPacket const& packet, QueueIterator it)
         {
            network::TCPResult result = packet.Write(*it, timeout);
            return !result.IsFailure();
         }

         void setupConnection(QueueIterator it, ClientInfo& clientInfo)
         {
            network::buffer data(40);
            NetworkPacket   ackAck = NetworkPacket::Read(*it, data, timeout);
            if (ackAck.command != Commands::Setup::Ack)
               throw std::runtime_error(Commands::Setup::Ack +
                                        " not received. Protocol violated");
            NetworkPacket const ackSyn(Commands::Setup::Syn);
            if (!WritePacket(ackSyn, it))
               throw std::runtime_error(Commands::Setup::Syn +
                                        " not sent. Protocol violated");

            NetworkPacket const clientId = NetworkPacket::Read(*it, data, timeout);
            if (clientId.command != Commands::Setup::ClientID)
               throw std::runtime_error(Commands::Setup::ClientID +
                                        " not received. Protocol violated");
            clientInfo.clientId = std::string(clientId.data);

            NetworkPacket const join(Commands::Setup::Join);
            if (!WritePacket(join, it))
               throw std::runtime_error(Commands::Setup::Join +
                                        " not sent. Protocol violated");
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

            for (QueueIterator it = tcpPoll.dataQueue.out.begin();
                 it != tcpPoll.dataQueue.out.end();
                 ++it)
               if (clientInfos[it->fd].deadline.HealthCheckNeeded())
               {
                  // If this is not the cycle to terminate
                  if (!clientInfos[it->fd].scheduleTermination)
                  {
                     // Send Down Check first
                     NetworkPacket downCheck(Commands::DownDetect::DownCheck);
                     downCheck.Write(*it, timeout);
                     clientInfos[it->fd].scheduleTermination = true;
                  }
                  // It is enabled
                  // So if we end up here again
                  // It means that the Health Check was not reset during
                  // So no message came
                  // Hence kill
                  else
                     closeConnection(it);
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
            for (QueueIterator it = tcpPoll.dataQueue.out.begin();
                 it != tcpPoll.dataQueue.out.end();
                 ++it)
            {
               if (it->revents & POLLHUP || it->revents & POLLNVAL ||
                   clientInfos[it->fd].deadlineBreach())
                  return closeConnection(it);
               if (it->revents & POLLIN)
               {
                  char    arr[2];
                  ssize_t bytes = pc::network::TCP::recvRaw(it->fd, arr, 1, MSG_PEEK);
                  if (bytes <= 0)
                     return closeConnection(it);
                  executeCallback(it);
               }
            }
         }
      };
   } // namespace protocol
} // namespace pc
