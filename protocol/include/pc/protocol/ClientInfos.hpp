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

         typedef threads::MutexGuard MutexGuard;

         typedef std::vector<pollfd> PollVec;

       public:
         typedef PollVec::iterator PollConstIterator;

       private:
         ClientInfoMap          clientInfos;
         mutable threads::Mutex mutex;
         PollVec                polls;
         bool                   updateIssued;

       public:
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
         void insert(int const socket, ClientResponseCallback callback)
         {
            updateIssued = true;
            MutexGuard lock(mutex);
            clientInfos.insert(std::make_pair(socket, ClientInfo(socket, callback)));
         }
         ClientInfo get(int const socket) const
         {
            MutexGuard     lock(mutex);
            const_iterator it = clientInfos.find(socket);
            assert(it != clientInfos.end());
            assert(it->first == socket);
            return it->second;
         }
         ClientInfo& get(int const socket)
         {
            MutexGuard lock(mutex);
            iterator   it = clientInfos.find(socket);
            assert(it != clientInfos.end());
            assert(it->first == socket);
            return it->second;
         }
         template <typename UniqueSockets>
         void FilterSocketsToTerminate(UniqueSockets& socketsSelected)
         {
            for (typename UniqueSockets::const_iterator it = socketsSelected.begin();
                 it != socketsSelected.end();)
            {
               int const socket       = *it;
               bool      terminateNow = false;
               {
                  iterator it = clientInfos.find(socket);
                  if (it == clientInfos.end())
                     terminateNow = false;
                  else
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
         PollVec& Polls()
         {
            MutexGuard lock(mutex);
            if (updateIssued)
            {
               polls.clear();
               for (const_iterator it = clientInfos.begin(); it != clientInfos.end();
                    ++it)
               {
                  pollfd pollAdd;
                  pollAdd.fd     = it->first;
                  pollAdd.events = POLLIN | POLLOUT;
                  polls.push_back(pollAdd);
               }
               updateIssued = false;
            }
            return polls;
         }

         void Execute()
         {
            pc::threads::MutexGuard lock(mutex);
            for (iterator it = clientInfos.begin(); it != clientInfos.end(); ++it)
               it->second.executeCallbacks();
         }

         std::size_t size() const
         {
            MutexGuard lock(mutex);
            return clientInfos.size();
         }
         template<typename Buffer>
         ClientPollResult OnReadPoll(::pollfd poll, Buffer& buffer)
         {
            MutexGuard lock(mutex);
            iterator   it = clientInfos.find(poll.fd);
            if (it == clientInfos.end())
            {
               ClientPollResult result;
               result.terminate = true;
               return result;
            }
            return it->second.OnReadPoll(poll, buffer);
         }
      };
   } // namespace protocol
} // namespace pc
