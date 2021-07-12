#pragma once

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/deadliner/MostRecentTimestamps.hpp>
#include <pc/network/TCPPoll.hpp>
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
         typedef ClientInfos::PollConstIterator          PollConstIterator;
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

         void Add(int const socket, ClientResponseCallback callback)
         {
            clientInfos.insert(socket, callback);
            config->balancer->incPriority(balancerIndex,
                                          clientInfos.get(socket).DeadlineMaxCount());
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
            closeSocketConnections(socketsSelected);
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
            std::vector<pollfd>& polls = clientInfos.Polls();
            if (network::TCPPoll::poll(polls, timeout) == 0)
               // Timeout
               return;
            UniqueSockets socketsWithReadSuccess;
            UniqueSockets socketsToTerminate;

            // Check poll results
            for (PollConstIterator it = polls.begin(); it != polls.end(); ++it)
            {
               ::pollfd const   poll   = *it;
               ClientPollResult result = clientInfos.OnReadPoll(poll, buffer);
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
         void Write()
         {
            return clientInfos.Write(timeout);
         }
      };
   } // namespace protocol
} // namespace pc
