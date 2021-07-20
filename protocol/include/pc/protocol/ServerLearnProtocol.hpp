#pragma once

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/deadliner/MostRecentTimestamps.hpp>
#include <pc/poll/Poll.hpp>
#include <pc/protocol/ClientInfos.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <pc/protocol/LearnProtocol.hpp>

namespace pc
{
   namespace protocol
   {
      class ServerLearnProtocol : public LearnProtocol
      {
         typedef deadliner::MostRecentTimestamps MostRecentTimestamps;

         ClientInfos          clientInfos;
         MostRecentTimestamps mostRecentTimestamps;

       public:
         std::size_t balancerIndex;

         ServerLearnProtocol(std::time_t const timeout = 10) : LearnProtocol(timeout) {}

         template <typename Balancer>
         void Add(Balancer&         balancer,
                  int const         socket,
                  std::size_t const DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT)
         {
            return Add(balancer, ClientInfo(socket, DeadlineMaxCount));
         }
         template <typename Balancer>
         void Add(Balancer& balancer, ClientInfo const& clientInfo)
         {
            clientInfos.insert(clientInfo);
            balancer.incPriority(balancerIndex, clientInfo.DeadlineMaxCount());
            mostRecentTimestamps.updateSingle(clientInfo.socket);
         }
         bool empty() const
         {
            return clientInfos.empty();
         }
         template <typename UniqueSockets>
         static void CloseSockets(UniqueSockets const& socketsSelected)
         {
            for (typename UniqueSockets::const_iterator it = socketsSelected.begin();
                 it != socketsSelected.end();
                 ++it)
               // Just close the socket
               // They will error out later
               network::Socket::close(*it);
         }
         template <typename UniqueSockets>
         void ExecHealthCheck(UniqueSockets& socketsSelected)
         {
            if (mostRecentTimestamps.size() == 0)
               return;
            UniqueSockets socketsSelectedLocal;
            // Use local UniqueSockets variable
            // Because FilterSocketsToTerminate performs checks on every socket
            // In the list
            {
               mostRecentTimestamps.getSocketsLessThanTimestamp(socketsSelectedLocal,
                                                                timeout);
               if (socketsSelectedLocal.empty())
                  return;
               clientInfos.FilterSocketsToTerminate(socketsSelectedLocal);
            }
            socketsSelected.insert(socketsSelectedLocal.begin(),
                                   socketsSelectedLocal.end());
         }
         std::size_t size() const
         {
            return clientInfos.size();
         }
         template <typename Balancer>
         ClientInfo RemoveOldestClientAndReturn(Balancer& balancer)
         {
            ClientInfo clientInfo = clientInfos.RemoveOldestClientAndReturn();
            balancer.decPriority(balancerIndex, clientInfo.DeadlineMaxCount());
            mostRecentTimestamps.removeSingle(clientInfo.socket);
            return clientInfo;
         }

         void Execute(ClientResponseCallback& callback)
         {
            return clientInfos.Execute(callback);
         }
         template <typename UniqueSockets, typename Buffer>
         void PollRead(Buffer& buffer, UniqueSockets& socketsToTerminate)
         {
            clientInfos.Update();
            UniqueSockets socketsWeReadAt;
            clientInfos.OnReadPoll(socketsWeReadAt, socketsToTerminate, buffer, timeout);
            // Upon success
            // Update timestamps
            mostRecentTimestamps.updateFor(socketsWeReadAt);
         }
         template <typename UniqueSockets, typename Balancer, typename DownCallback>
         void CloseSocketConnections(Balancer&      balancer,
                                     UniqueSockets& socketsToRemove,
                                     DownCallback&  downCallback)
         {
            if (socketsToRemove.empty())
               return;
            // Remove all socket based connections
            // From polls
            clientInfos.close(socketsToRemove, balancer, balancerIndex);
            if (socketsToRemove.empty())
               return;
            // Close all sockets
            // Remove them from timestamp
            // Call callback
            mostRecentTimestamps.remove(socketsToRemove);
            // Notify user when a client goes down
            downCallback(balancerIndex, socketsToRemove.size());
         }
         void Write()
         {
            return clientInfos.Write();
         }
      };
   } // namespace protocol
} // namespace pc
