#pragma once

#include <pc/protocol/ClientInfo.hpp>
#include <pc/protocol/ClientPollResult.hpp>
#include <pc/protocol/Config.hpp>
#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>
#include <tr1/unordered_map>
#include <vector>

namespace pc
{
   namespace protocol
   {
      class ClientInfos
      {
         typedef std::tr1::unordered_map<int /*Socket*/, ClientInfo> ClientInfoMap;

         typedef ClientInfoMap::iterator       iterator;
         typedef ClientInfoMap::const_iterator const_iterator;
         typedef pc::threads::MutexGuard       MutexGuard;
         typedef std::vector<pollfd>           PollVec;

       private:
         ClientInfoMap          clientInfos;
         mutable threads::Mutex mutex;
         bool                   updateIssued;

       public:
         PollVec PollsIn;

         template <typename UniqueSockets>
         void close(UniqueSockets&     socketsToRemove,
                    Config::balancerT* balancer,
                    std::size_t const  balancerIndex)
         {
            pc::threads::MutexGuard guard(mutex);
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
                  ++it;
               }
               // If it does not
               // Remove from Sockets to remove list
               else
                  it = socketsToRemove.erase(it);
            }
            updateIssued = true;
         }
         void insert(int const              socket,
                     ClientResponseCallback callback,
                     std::size_t const      DeadlineMaxCount = DEADLINE_MAX_COUNT_DEFAULT)
         {
            MutexGuard lock(mutex);
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
            MutexGuard lock(mutex);
            if (updateIssued)
            {
               PollsIn.clear();
               for (const_iterator it = clientInfos.begin(); it != clientInfos.end();
                    ++it)
               {
                  pollfd pollAdd;
                  pollAdd.fd     = it->first;
                  pollAdd.events = POLLIN;
                  PollsIn.push_back(pollAdd);
               }
               updateIssued = false;
            }
         }

         void Execute()
         {
            pc::threads::MutexGuard lock(mutex);
            for (iterator it = clientInfos.begin(); it != clientInfos.end(); ++it)
               it->second.executeCallbacks();
         }

         void Write(std::time_t timeout)
         {
            pc::threads::MutexGuard lock(mutex);
            for (iterator it = clientInfos.begin(); it != clientInfos.end(); ++it)
               it->second.WritePackets(timeout);
         }

         std::size_t size() const
         {
            MutexGuard lock(mutex);
            return clientInfos.size();
         }
         template <typename Buffer, typename UniqueSockets>
         void OnReadPoll(UniqueSockets& socketsWeReadAt,
                         UniqueSockets& socketsToTerminate,
                         Buffer&        buffer)
         {
            MutexGuard lock(mutex);

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
