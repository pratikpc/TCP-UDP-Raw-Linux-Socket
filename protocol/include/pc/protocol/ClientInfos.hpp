#pragma once

#include <pc/protocol/ClientInfo.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <pc/protocol/Config.hpp>
#include <tr1/unordered_map>
#include <vector>

#include <pc/thread/RWLock.hpp>
#include <pc/thread/RWReadGuard.hpp>
#include <pc/thread/RWWriteGuard.hpp>

namespace pc
{
   namespace protocol
   {
      class ClientInfos
      {
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfoMap;

         typedef ClientInfoMap::iterator       iterator;
         typedef ClientInfoMap::const_iterator const_iterator;
         typedef std::vector<pollfd>           PollVec;

         ClientInfoMap clientInfos;
         bool          updateIssued;

         mutable pc::threads::RWLock lock;

       public:
         PollVec PollsIn;

         template <typename UniqueSockets>
         void close(UniqueSockets&     socketsToRemove,
                    Config::balancerT* balancer,
                    std::size_t const  balancerIndex)
         {
            threads::RWWriteGuard guard(lock);
            for (typename UniqueSockets::iterator it = socketsToRemove.begin();
                 it != socketsToRemove.end();)
            {
               int const      socket = *it;
               const_iterator infoIt = clientInfos.find(socket);
               // Check if the socket exists first of all
               if (infoIt != clientInfos.end())
               {
                  balancer->decPriority(balancerIndex, infoIt->second.DeadlineMaxCount());
                  clientInfos.erase(socket);
                  ::close(socket);
                  ++it;
               }
               // If it does not
               // Remove from Sockets to remove list
               else
                  it = socketsToRemove.erase(it);
            }
            updateIssued = true;
         }
         void insert(int const               socket,
                     ClientResponseCallback& callback,
                     std::size_t const DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT)
         {
            threads::RWWriteGuard guard(lock);
            updateIssued = true;
            clientInfos.insert(
                std::make_pair(socket, ClientInfo(socket, callback, DeadlineMaxCount)));
         }
         template <typename UniqueSockets>
         void FilterSocketsToTerminate(UniqueSockets& socketsSelected)
         {
            if (socketsSelected.empty())
               return;
            for (typename UniqueSockets::const_iterator it = socketsSelected.begin();
                 it != socketsSelected.end();)
            {
               int const socket       = *it;
               bool      terminateNow = false;
               {
                  iterator it = clientInfos.find(socket);
                  if (it != clientInfos.end())
                     terminateNow = it->second.TerminateThisCycleOrNext();
               }
               // If this is not the cycle to terminate
               if (!terminateNow)
                  // Remove from the list
                  // Because all selected sockets will be terminated
                  it = socketsSelected.erase(it);
               else
                  ++it;
            }
         }
         void Update()
         {
            threads::RWReadGuard guard(lock);
            if (updateIssued)
            {
               PollsIn.clear();
               PollsIn.reserve(clientInfos.size());
               for (const_iterator it = clientInfos.begin(); it != clientInfos.end();
                    ++it)
               {
                  ::pollfd pollVar;
                  pollVar.fd     = it->first;
                  pollVar.events = POLLIN;
                  PollsIn.push_back(pollVar);
               }
               updateIssued = false;
            }
         }

         void Execute()
         {
            threads::RWReadGuard guard(lock);
            for (iterator it = clientInfos.begin(); it != clientInfos.end(); ++it)
               it->second.executeCallbacks();
         }

         void Write(std::time_t timeout)
         {
            threads::RWReadGuard guard(lock);
            for (iterator it = clientInfos.begin(); it != clientInfos.end(); ++it)
               it->second.WritePackets(timeout);
         }

         std::size_t size() const
         {
            threads::RWReadGuard guard(lock);
            return clientInfos.size();
         }
         template <typename Buffer, typename UniqueSockets>
         void OnReadPoll(UniqueSockets& socketsWeReadAt,
                         UniqueSockets& socketsToTerminate,
                         Buffer&        buffer)
         {
            threads::RWReadGuard guard(lock);

            PollVec::const_iterator pollIt = PollsIn.begin();
            for (ClientInfos::iterator clientIt = clientInfos.begin();
                 clientIt != clientInfos.end();
                 ++clientIt, ++pollIt)
            {
               ::pollfd const   poll   = *pollIt;
               ClientPollResult result = clientIt->second.OnReadPoll(poll, buffer);
               if (result.read)
                  socketsWeReadAt.insert(poll.fd);
               if (result.terminate)
                  socketsToTerminate.insert(poll.fd);
            }
         }
      };
   } // namespace protocol
} // namespace pc
