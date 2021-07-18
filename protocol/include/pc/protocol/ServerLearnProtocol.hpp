#pragma once

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/deadliner/MostRecentTimestamps.hpp>
#include <pc/poll/Poll.hpp>
#include <pc/protocol/ClientInfos.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <pc/protocol/Config.hpp>
#include <pc/protocol/LearnProtocol.hpp>

namespace pc
{
   namespace protocol
   {
      class ServerLearnProtocol : public LearnProtocol
      {
         typedef deadliner::MostRecentTimestamps         MostRecentTimestamps;
         typedef std::tr1::unordered_set<int /*socket*/> UniqueSockets;

         ClientInfos          clientInfos;
         MostRecentTimestamps mostRecentTimestamps;

         void closeSocketConnections(UniqueSockets& socketsToRemove)
         {
            if (socketsToRemove.empty())
               return;
            // Remove all socket based connections
            // From polls
            clientInfos.close(socketsToRemove, config->balancer, balancerIndex);
            // Close all sockets
            // Remove them from timestamp
            // Call callback
            mostRecentTimestamps.remove(socketsToRemove);
            // Notify user when a client goes down
            config->downCallback(balancerIndex, socketsToRemove.size());
         }

       public:
         std::size_t balancerIndex;
         Config*     config;

         void Add(int const               socket,
                  ClientResponseCallback& callback,
                  std::size_t const       DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT)
         {
            clientInfos.insert(socket, callback, DeadlineMaxCount);
            config->balancer->incPriority(balancerIndex, DeadlineMaxCount);
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
            clientInfos.FilterSocketsToTerminate(socketsSelected);
            for (UniqueSockets::const_iterator it = socketsSelected.begin();
                 it != socketsSelected.end();
                 ++it)
               // Just close the socket
               // They will error out later
               ::shutdown(*it, SHUT_RDWR);
         }
         std::size_t size() const
         {
            return clientInfos.size();
         }
         void Execute()
         {
            return clientInfos.Execute();
         }
         template <typename Buffer>
         void Poll(Buffer& buffer)
         {
            clientInfos.Update();
            std::vector<pollfd>& polls = clientInfos.PollsIn;
            if (poll::multiple(polls, timeout) == 0)
               // Timeout
               return;
            UniqueSockets socketsToTerminate;
            UniqueSockets socketsWeReadAt;

            clientInfos.OnReadPoll(socketsWeReadAt, socketsToTerminate, buffer);
            // Upon success
            // Update timestamps
            mostRecentTimestamps.updateFor(socketsWeReadAt);
            // Upon termination
            closeSocketConnections(socketsToTerminate);
         }
         void Write()
         {
            return clientInfos.Write(timeout);
         }
      };
   } // namespace protocol
} // namespace pc
