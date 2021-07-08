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

         void closeSocketConnections(UniqueSockets& socketsToRemove)
         {
            if (socketsToRemove.empty())
               return;
            // Remove all socket based connections
            // From polls
            {
               pc::threads::MutexGuard guard(pollsMutex);
               for (UniqueSockets::iterator it = socketsToRemove.begin();
                    it != socketsToRemove.end();)
               {
                  int const socket = *it;
                  // Check if the socket exists first of all
                  if (clientInfos.find(socket) != clientInfos.end())
                  {
                     clientInfos.erase(socket);
                     config->balancer->decPriority(
                         balancerIndex, clientInfos[socket].DeadlineMaxCount());
                     ++it;
                  }
                  // If it does not
                  // Remove from Sockets to remove list
                  else
                     it = socketsToRemove.erase(it);
               }
               tcpPoll.remove(socketsToRemove);
            }

            // Close all sockets
            // Remove them from timestamp
            // Call callback
            mostRecentTimestamps.remove(socketsToRemove);
            // Notify user when a client goes down
            config->downCallback(balancerIndex);
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
                                             clientInfos[socket].DeadlineMaxCount());
            }
            mostRecentTimestamps.updateSingle(socket);
         }

         void execHealthCheck()
         {
            if (mostRecentTimestamps.size() == 0)
               return;
            UniqueSockets socketsSelected =
                mostRecentTimestamps.getSocketsLessThanTimestamp<UniqueSockets>(timeout);
            if (socketsSelected.empty())
               return;
            for (UniqueSockets::const_iterator it = socketsSelected.begin();
                 it != socketsSelected.end();)
            {
               int const socket       = *it;
               bool      terminateNow = false;
               {
                  pc::threads::MutexGuard guard(pollsMutex);
                  terminateNow = clientInfos[socket].TerminateThisCycleOrNext();
               }
               // If this is not the cycle to terminate
               if (!terminateNow)
                  // Remove from the list
                  // Because all selected sockets will be terminated
                  it = socketsSelected.erase(it);
               else
                  ++it;
            }
            closeSocketConnections(socketsSelected);
         }
         std::size_t size() const
         {
            return tcpPoll.size();
         }
         void Execute()
         {
            pc::threads::MutexGuard lock(pollsMutex);
            for (ClientInfos::iterator it = clientInfos.begin(); it != clientInfos.end();
                 ++it)
               it->second.executeCallbacks();
         }
         void Poll()
         {
            {
               pc::threads::MutexGuard lock(pollsMutex);
               tcpPoll.PerformUpdate();
            }
            if (tcpPoll.poll(timeout) == 0)
               // Timeout
               return;
            UniqueSockets socketsWithReadSuccess;
            UniqueSockets socketsToTerminate;

            // Check poll results
            for (QueueIterator it = tcpPoll.begin(); it != tcpPoll.end(); ++it)
            {
               ::pollfd const   poll   = *it;
               ClientPollResult result = clientInfos[poll.fd].OnPoll(poll, timeout);
               if (result.read)
                  socketsWithReadSuccess.insert(poll.fd);
               if (result.terminate)
                  socketsToTerminate.insert(poll.fd);
            }
            // Upon success
            // Update timestamps
            mostRecentTimestamps.updateFor(socketsWithReadSuccess);
            // Upon termination
            closeSocketConnections(socketsToTerminate);
         }
      };
   } // namespace protocol
} // namespace pc
