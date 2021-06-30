#pragma once

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>
#include <vector>

namespace pc
{
   namespace balancer
   {
      class priority_queue
      {
         std::vector<std::size_t> sizes;

         pc::threads::Mutex mutex;
         std::size_t        lowestSizeIndex;

       public:
         friend std::ostream& operator<<(std::ostream& os, priority_queue& queue)
         {
            pc::threads::MutexGuard guard(queue.mutex);
            for (std::size_t i = 0; i < queue.sizes.size(); ++i)
               os << queue.sizes[i] << " : ";
            return os;
         }
         priority_queue(std::size_t maxSize) : sizes(maxSize), lowestSizeIndex(0) {}

         std::size_t operator*()
         {
            pc::threads::MutexGuard guard(mutex);
            return lowestSizeIndex;
         }
         priority_queue& operator++()
         {
            {
               pc::threads::MutexGuard guard(mutex);
               ++sizes[lowestSizeIndex];
               lowestSizeIndex = MinIdx(lowestSizeIndex);
            }
            return *this;
         }
         void decPriority(std::size_t index)
         {
            pc::threads::MutexGuard guard(mutex);

            if (sizes[index] != 0)
               --sizes[index];
         }
         priority_queue& MaxCount(std::size_t MaxCount)
         {
            pc::threads::MutexGuard guard(mutex);

            sizes.resize(MaxCount);
            return *this;
         }
         std::size_t MaxCount()
         {
            return sizes.size();
         }

         std::size_t MinIdx(int minIdx)
         {
            for (std::size_t i = 0; i < sizes.size(); ++i)
            {
               // Early return
               // As we know min value
               if (sizes[i] == 0)
                  return i;
               if (sizes[i] < sizes[minIdx])
                  return i;
            }
            return minIdx;
         }
      };
   } // namespace balancer
} // namespace pc