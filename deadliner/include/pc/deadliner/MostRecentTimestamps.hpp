#pragma once

#include <pc/OrderedHashSet.hpp>
#include <pc/timer/timer.hpp>

#ifndef PC_USE_SPINLOCKS
#   include <pc/thread/Mutex.hpp>
#   include <pc/thread/MutexGuard.hpp>
#else
#   include <pc/thread/spin/SpinGuard.hpp>
#   include <pc/thread/spin/SpinLock.hpp>
#endif

namespace pc
{
   namespace deadliner
   {
      struct MostRecentTimestamps
      {
         typedef pc::OrderedHashSet<int /*socket*/, std::time_t> Timestamps;

         typedef Timestamps::iterator       iterator;
         typedef Timestamps::const_iterator const_iterator;

       private:
         Timestamps timestamps;

#ifndef PC_USE_SPINLOCKS
         mutable pc::threads::Mutex      mostRecentTimestampsLock;
         typedef pc::threads::MutexGuard LockGuard;
#else
         mutable pc::threads::SpinLock  mostRecentTimestampsLock;
         typedef pc::threads::SpinGuard LockGuard;
#endif

       public:
         iterator begin()
         {
            return timestamps.begin();
         }
         const_iterator begin() const
         {
            return timestamps.begin();
         }
         const_iterator end() const
         {
            return timestamps.end();
         }

         std::size_t size() const
         {
            LockGuard guard(mostRecentTimestampsLock);
            return timestamps.size();
         }

         template <typename Sockets>
         void updateFor(Sockets const& sockets, std::time_t time = timer::seconds())
         {
            if (sockets.empty())
               return;
            LockGuard guard(mostRecentTimestampsLock);
            for (typename Sockets::const_iterator it = sockets.begin();
                 it != sockets.end();
                 ++it)
               // Add socket and iterator to current index
               // Makes removal easy
               timestamps.insert(*it, time);
         }

         void updateSingle(int socket, std::time_t time = timer::seconds())
         {
            LockGuard guard(mostRecentTimestampsLock);
            timestamps.insert(socket, time);
         }
         template <typename Sockets>
         void getSocketsLessThanTimestamp(Sockets&          sockets,
                                          std::time_t const timeout) const
         {
            std::time_t const now = timer::seconds();

            LockGuard guard(mostRecentTimestampsLock);
            for (const_iterator it = begin(); it != end(); ++it)
            {
               int const socket = it->first;
               // Input is sorted by ascending order
               if ((now - it->second /*Time then*/) < timeout)
                  break;
               sockets.insert(socket);
            }
         }
         void Reset(int socket)
         {
            return updateSingle(socket, 0);
         }
         template <typename Sockets>
         void remove(Sockets const& sockets)
         {
            LockGuard guard(mostRecentTimestampsLock);
            for (typename Sockets::const_iterator it = sockets.begin();
                 it != sockets.end();
                 ++it)
               // Remove socket
               timestamps.remove(*it);
         }
         void removeSingle(int const socket)
         {
            LockGuard guard(mostRecentTimestampsLock);
            // Remove socket
            timestamps.remove(socket);
         }
      };
   } // namespace deadliner
} // namespace pc
