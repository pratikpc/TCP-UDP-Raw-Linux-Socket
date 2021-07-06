#pragma once

#include <tr1/unordered_map>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/deadliner/MostRecentTimestamps.hpp>
#include <pc/network/TCPPoll.hpp>
#include <pc/protocol/ClientInfo.hpp>
#include <pc/protocol/Config.hpp>
#include <pc/protocol/LearnProtocol.hpp>

namespace pc
{
   namespace protocol
   {
      class ServerLearnProtocol : public LearnProtocol
      {
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfos;
         typedef DataQueue<pollfd>::QueueVec::iterator               QueueIterator;
         typedef deadliner::MostRecentTimestamps<DataQueue<pollfd>::QueueVec>
             MostRecentTimestamps;

         ClientInfos        clientInfos;
         network::TCPPoll   tcpPoll;
         pc::threads::Mutex pollsMutex;

         MostRecentTimestamps mostRecentTimestamps;
         pc::threads::Mutex   mostRecentTimestampsMutex;

         void closeConnection(QueueIterator it)
         {
            int socket = it->fd;
            close(socket);
            {
               pc::threads::MutexGuard guard(pollsMutex);
               // Check if the socket exists first of all
               // If it does not
               // Do nothing
               if (clientInfos.find(socket) == clientInfos.end())
                  return;

               std::size_t indexErase = it - tcpPoll.dataQueue.out.begin();
               config->balancer->decPriority(balancerIndex,
                                             clientInfos[socket].deadline.MaxCount());
               clientInfos.erase(socket);
               // Delete current element
               tcpPoll -= indexErase;
            }
            // {
            //    pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
            //    mostRecentTimestamps -= socket;
            // }
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
            {
               pc::threads::MutexGuard guard(pollsMutex);
               clientInfos[it->fd].scheduleTermination = false;
            }

            if (readPacket.command == Commands::MajorErrors::SocketClosed)
               return closeConnection(it);
            if (readPacket.command != Commands::Send)
               return;
            NetworkSendPacket const packet = executeCallback(it->fd, readPacket);
            if (packet.command == Commands::Send)
               if (!WritePacket(packet, it))
                  closeConnection(it);
            {
               pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
               // Add socket and iterator to current index
               // Makes removal easy
               mostRecentTimestamps.insert(it->fd, it);
            }
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
            std::time_t const now = timer::seconds();

            pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
            for (MostRecentTimestamps::iterator it = mostRecentTimestamps.begin();
                 it != mostRecentTimestamps.end();)
            {
               int socket = it->first;
               // Input is sorted by ascending order
               if ((now - it->second.first /*Time then*/) <
                   config->healthCheckDurationToPerform.maxDurationDifference)
                  break;
               // If this is not the cycle to terminate
               if (clientInfos[socket].scheduleTermination)
               {
                  closeConnection(it->second.second);
                  it = mostRecentTimestamps.removeAndIterate(it);
               }
               // It is enabled
               // So if we end up here again
               // It means that the Health Check was not reset during
               // So no message came
               // Hence kill
               else
               {
                  // Schedule termination
                  clientInfos[socket].scheduleTermination = true;
                  ++it;
               }
            }
         }

         std::size_t size() const
         {
            return tcpPoll.size();
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
                  if (!pc::network::TCP::containsDataToRead(it->fd))
                     return closeConnection(it);
                  executeCallback(it);
               }
            }
         }
      };
   } // namespace protocol
} // namespace pc
