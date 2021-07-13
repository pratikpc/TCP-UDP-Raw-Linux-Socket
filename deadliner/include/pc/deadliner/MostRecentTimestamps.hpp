#pragma once

#include <pc/OrderedHashSet.hpp>
#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>
#include <pc/timer/timer.hpp>

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
         Timestamps                 timestamps;
         mutable pc::threads::Mutex mostRecentTimestampsMutex;

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
            pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
            return timestamps.size();
         }

         template <typename Sockets>
         void updateFor(Sockets const& sockets, std::time_t time = timer::seconds())
         {
            if (sockets.empty())
               return;
            pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
            for (typename Sockets::const_iterator it = sockets.begin();
                 it != sockets.end();
                 ++it)
               // Add socket and iterator to current index
               // Makes removal easy
               timestamps.insert(*it, time);
         }

         void updateSingle(int socket, std::time_t time = timer::seconds())
         {
            pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
            timestamps.insert(socket, time);
         }
         template <typename Sockets>
         Sockets getSocketsLessThanTimestamp(std::time_t const timeout)
         {
            Sockets           sockets;
            std::time_t const now = timer::seconds();

            pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
            for (const_iterator it = begin(); it != end(); ++it)
            {
               int socket = it->first;
               // Input is sorted by ascending order
               if ((now - it->second /*Time then*/) < timeout)
                  break;
               sockets.insert(socket);
            }
            return sockets;
         }
         void Reset(int socket)
         {
            return updateSingle(socket, 0);
         }
         template <typename Sockets>
         MostRecentTimestamps& remove(Sockets const& sockets)
         {
            pc::threads::MutexGuard guard(mostRecentTimestampsMutex);
            for (typename Sockets::const_iterator it = sockets.begin();
                 it != sockets.end();
                 ++it)
               // Remove socket
               timestamps.remove(*it);
            return *this;
         }
      };
   } // namespace deadliner
} // namespace pc
