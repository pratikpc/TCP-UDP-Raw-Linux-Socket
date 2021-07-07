#pragma once

#include <tr1/unordered_map>
#include <tr1/unordered_set>

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
         typedef deadliner::MostRecentTimestamps                     MostRecentTimestamps;
         typedef std::tr1::unordered_set<int /*socket*/>             UniqueSockets;

         ClientInfos        clientInfos;
         network::TCPPoll   tcpPoll;
         pc::threads::Mutex pollsMutex;

         MostRecentTimestamps mostRecentTimestamps;
         pc::threads::Mutex   mostRecentTimestampsMutex;

         template <typename Iterable>
         void closeSocketConnections(Iterable const& socketsToRemove)
         {
            if (socketsToRemove.empty())
               return;
            // Remove all socket based connections
            // From polls
            {
               pc::threads::MutexGuard guard(pollsMutex);
               for (QueueIterator it = tcpPoll.begin(); it != tcpPoll.end(); ++it)
               {
                  int const socket = it->fd;
                  // Check if the socket exists first of all
                  // If it does not
                  // Do nothing
                  if (socketsToRemove.find(socket) == socketsToRemove.end())
                     continue;
                  clientInfos.erase(socket);

                  std::size_t const indexErase = it - tcpPoll.begin();
                  // Delete current element
                  tcpPoll.removeAtIndex(indexErase);

                  config->balancer->decPriority(balancerIndex,
                                                clientInfos[socket].deadline.MaxCount());
               }
            }

            // Close all sockets
            // Remove them from timestamp
            // Call callback
            for (typename Iterable::const_iterator it = socketsToRemove.begin();
                 it != socketsToRemove.end();
                 ++it)
            {
               int socket = *it;
               ::close(socket);
               {
                  pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
                  mostRecentTimestamps.remove(socket);
               }
               // Notify user when a client goes down
               config->downCallback(balancerIndex);
            }
         }

         NetworkSendPacket executeCallbackLockByPolls(int                  socket,
                                                      NetworkPacket const& readPacket)
         {
            NetworkSendPacket packet =
                clientInfos[socket].callback(readPacket, clientInfos[socket]);
            ++clientInfos[socket].deadline;
            return packet;
         }
         network::TCPResult executeCallbackLockByPolls(int socket)
         {
            if (!clientInfos[socket].hasClientId())
               return setupConnection(socket, clientInfos[socket]);
            network::buffer buffer(UINT16_MAX);
            NetworkPacket   readPacket = NetworkPacket::Read(socket, buffer, 0);
            clientInfos[socket].scheduleTermination = false;
            network::TCPResult result;
            if (readPacket.command == Commands::MajorErrors::SocketClosed)
            {
               result.SocketClosed = true;
               return result;
            }
            if (readPacket.command != Commands::Send)
            {
               result.NoOfBytes = 0;
               return result;
            }
            NetworkSendPacket const packet =
                executeCallbackLockByPolls(socket, readPacket);
            if (packet.command == Commands::Send)
            {
               ++clientInfos[socket].deadline;
               return WritePacket(packet, socket);
            }
            return result;
         }

         network::TCPResult WritePacket(NetworkPacket const& packet, int const socket)
         {
            return packet.Write(socket, timeout);
         }

         network::TCPResult setupConnection(int const socket, ClientInfo& clientInfo)
         {
            network::TCPResult result;
            network::buffer    data(40);
            NetworkPacket      ackAck = NetworkPacket::Read(socket, data, timeout);
            if (ackAck.command != Commands::Setup::Ack)
            {
               result.SocketClosed = true;
               return result;
            }
            network::TCPResult const ackSynResult =
                WritePacket(NetworkPacket(Commands::Setup::Syn), socket);
            if (ackSynResult.IsFailure())
               return ackSynResult;

            NetworkPacket const clientId = NetworkPacket::Read(socket, data, timeout);
            if (clientId.command != Commands::Setup::ClientID)
            {
               network::TCPResult result;
               result.SocketClosed = true;
               return result;
            }
            clientInfo.clientId = std::string(clientId.data);

            NetworkPacket const      join(Commands::Setup::Join);
            network::TCPResult const joinResult =
                WritePacket(NetworkPacket(Commands::Setup::Join), socket);
            if (joinResult.IsFailure())
               return joinResult;
            {
               // As the balancer element is common
               // and shared across protocols
               // Make the balancer updation guard
               // static
               static pc::threads::Mutex balancerPriorityUpdationMutex;
               pc::threads::MutexGuard   guard(balancerPriorityUpdationMutex);
               std::size_t               newDeadlineMaxCount =
                   config->ExtractDeadlineMaxCountFromDatabase(clientInfo.clientId);
               config->balancer->setPriority(balancerIndex,
                                             // Update priority for given element
                                             (*config->balancer)[balancerIndex] -
                                                 clientInfo.deadline.MaxCount() +
                                                 newDeadlineMaxCount);
               clientInfo.changeMaxCount(newDeadlineMaxCount);
            }
            return result;
         }

       public:
         std::size_t balancerIndex;
         Config*     config;

         void Add(int const socket, ClientResponseCallback callback)
         {
            ClientInfo clientInfo = ClientInfo::createClientInfo(socket, callback);
            {
               // Protect this during multithreaded access
               pc::threads::MutexGuard lock(pollsMutex);
               tcpPoll.addSocketToPoll(socket);
               clientInfos[socket] = clientInfo;
               config->balancer->incPriority(balancerIndex,
                                             clientInfos[socket].deadline.MaxCount());
               ++clientInfos[socket].deadline;
            }
            {
               pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
               mostRecentTimestamps.updateFor(socket);
            }
         }

         void execHealthCheck()
         {
            std::time_t const now = timer::seconds();

            UniqueSockets socketsSelected;

            if (mostRecentTimestamps.size() == 0)
               return;
            {
               pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
               for (MostRecentTimestamps::const_iterator it =
                        mostRecentTimestamps.begin();
                    it != mostRecentTimestamps.end();
                    ++it)
               {
                  int socket = it->first;
                  // Input is sorted by ascending order
                  if ((now - it->second /*Time then*/) < timeout)
                     break;
                  socketsSelected.insert(socket);
               }
            }
            for (UniqueSockets::const_iterator it = socketsSelected.begin();
                 it != socketsSelected.end();
                 ++it)
            {
               int const socket    = *it;
               bool      terminate = false;
               {
                  pc::threads::MutexGuard guard(pollsMutex);
                  terminate = clientInfos[socket].scheduleTermination;
               }
               // If this is not the cycle to terminate
               if (!terminate)
               {
                  {
                     pc::threads::MutexGuard guard(pollsMutex);
                     clientInfos[socket].scheduleTermination = true;
                  }
                  // Remove from the list
                  // Because all selected sockets will be terminated
                  it = socketsSelected.erase(it);
               }
            }
            closeSocketConnections(socketsSelected);
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
            UniqueSockets socketsToRemove;
            UniqueSockets socketsWithSuccess;
            // Check poll results
            {
               pc::threads::MutexGuard lock(pollsMutex);
               for (QueueIterator it = tcpPoll.begin(); it != tcpPoll.end(); ++it)
               {
                  int const socket = it->fd;
                  if (it->revents & POLLHUP || it->revents & POLLNVAL ||
                      clientInfos[socket].deadlineBreach())
                     socketsToRemove.insert(socket);
                  else if (it->revents & POLLIN)
                  {
                     if (pc::network::TCP::containsDataToRead(socket))
                     {
                        network::TCPResult result = executeCallbackLockByPolls(socket);
                        if (result.SocketClosed)
                           socketsToRemove.insert(socket);
                        // If not failure, this operation was a success
                        else if (!result.IsFailure())
                           socketsWithSuccess.insert(socket);
                     }
                     else
                        socketsToRemove.insert(socket);
                  }
               }
            }
            // Upon success
            // Update timestamps
            {
               pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
               for (UniqueSockets::const_iterator it = socketsWithSuccess.begin();
                    it != socketsWithSuccess.end();
                    ++it)
                  // Add socket and iterator to current index
                  // Makes removal easy
                  mostRecentTimestamps.updateFor(*it /*socket*/);
            }
            closeSocketConnections(socketsToRemove);
         }
      };
   } // namespace protocol
} // namespace pc
