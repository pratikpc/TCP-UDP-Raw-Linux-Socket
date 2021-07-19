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
         typedef deadliner::MostRecentTimestamps MostRecentTimestamps;

         ClientInfos          clientInfos;
         MostRecentTimestamps mostRecentTimestamps;

         template <typename UniqueSockets>
         void closeSocketConnections(UniqueSockets& socketsToRemove)
         {
            if (socketsToRemove.empty())
               return;
            // Remove all socket based connections
            // From polls
            clientInfos.close(socketsToRemove, config.balancer, balancerIndex);
            // Close all sockets
            // Remove them from timestamp
            // Call callback
            mostRecentTimestamps.remove(socketsToRemove);
            // Notify user when a client goes down
            config.downCallback(balancerIndex, socketsToRemove.size());
         }

       public:
         std::size_t balancerIndex;
         Config&     config;

         ServerLearnProtocol(Config& config, std::time_t const timeout = 10) :
             LearnProtocol(timeout), config(config)
         {
         }

         void Add(int const         socket,
                  std::size_t const DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT)
         {
            clientInfos.insert(socket, DeadlineMaxCount);
            config.balancer.incPriority(balancerIndex, DeadlineMaxCount);
            mostRecentTimestamps.updateSingle(socket);
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
         void Execute(ClientResponseCallback& callback)
         {
            return clientInfos.Execute(callback);
         }
         template <typename UniqueSockets, typename Buffer>
         void Poll(Buffer& buffer)
         {
            clientInfos.Update();
            UniqueSockets socketsToTerminate;
            UniqueSockets socketsWeReadAt;

            clientInfos.OnReadPoll(socketsWeReadAt, socketsToTerminate, buffer, timeout);
            // Upon success
            // Update timestamps
            mostRecentTimestamps.updateFor(socketsWeReadAt);
            // Upon termination
            closeSocketConnections(socketsToTerminate);
         }
         void Write()
         {
            return clientInfos.Write();
         }
      };
   } // namespace protocol
} // namespace pc
