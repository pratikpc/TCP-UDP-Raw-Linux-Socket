#pragma once

#include <pc/OrderedHashSet.hpp>
#include <pc/timer/timer.hpp>

namespace pc
{
   namespace deadliner
   {
      template <typename DataQueue>
      struct MostRecentTimestamps
      {
         typedef pc::OrderedHashSet<
             int /*socket*/,
             std::pair<std::time_t, typename DataQueue::iterator> /* */>
                                                     Timestamps;
         typedef typename Timestamps::iterator       iterator;
         typedef typename Timestamps::const_iterator const_iterator;

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

         MostRecentTimestamps& insert(int socket, typename DataQueue::iterator it)
         {
            timestamps.insert(socket, std::make_pair(timer::seconds(), it));
            return *this;
         }
         iterator removeAndIterate(iterator it)
         {
            return timestamps.removeAndIterate(it);
         }
      };
   } // namespace deadliner
} // namespace pc
