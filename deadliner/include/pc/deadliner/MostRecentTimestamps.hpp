#pragma once

#include <pc/OrderedHashSet.hpp>
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
         Timestamps timestamps;

       public:
         iterator begin()
         {
            return timestamps.begin();
         }
         const_iterator begin() const
         {
            return timestamps.begin();
         }
         const_iterator end()
         {
            return timestamps.end();
         }
         const_iterator end() const
         {
            return timestamps.end();
         }

         std::size_t size() const
         {
            return timestamps.size();
         }

         MostRecentTimestamps& insert(int socket)
         {
            timestamps.insert(socket, timer::seconds());
            return *this;
         }
         iterator removeAndIterate(iterator it)
         {
            return timestamps.removeAndIterate(it);
         }
         void remove(int socket)
         {
            return timestamps.remove(socket);
         }
      };
   } // namespace deadliner
} // namespace pc
