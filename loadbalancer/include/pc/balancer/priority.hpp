#pragma once

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

#include <ostream>
#include <vector>

namespace pc
{
   namespace balancer
   {
      class priority
      {
         std::vector<std::size_t>   sizes;
         mutable pc::threads::Mutex mutex;

         std::size_t lowestSizeIndex;

       public:
         friend std::ostream& operator<<(std::ostream& os, priority& queue)
         {
            pc::threads::MutexGuard guard(queue.mutex);
            for (std::size_t i = 0; i < queue.sizes.size(); ++i)
               os << queue.sizes[i] << " : ";
            return os;
         }
         priority(std::size_t maxSize) : sizes(maxSize), lowestSizeIndex(0) {}

         std::size_t operator*() const
         {
            pc::threads::MutexGuard guard(mutex);
            return lowestSizeIndex;
         }
         priority& operator++()
         {
            {
               pc::threads::MutexGuard guard(mutex);
               ++sizes[lowestSizeIndex];
               lowestSizeIndex = MinIdx(lowestSizeIndex);
            }
            return *this;
         }
         void incPriority(std::size_t index, std::size_t by = 1)
         {
            pc::threads::MutexGuard guard(mutex);
            sizes[index] += by;
            lowestSizeIndex = MinIdx(lowestSizeIndex);
         }
         void decPriority(std::size_t index, std::size_t by = 1)
         {
            pc::threads::MutexGuard guard(mutex);
            if (sizes[index] >= by)
            {
               sizes[index] -= by;
               lowestSizeIndex = MinIdx(lowestSizeIndex);
            }
         }
         void setPriority(std::size_t index, std::size_t value)
         {
            pc::threads::MutexGuard guard(mutex);
            sizes[index]    = value;
            lowestSizeIndex = MinIdx(lowestSizeIndex);
         }
         std::size_t operator[](std::size_t index) const
         {
            pc::threads::MutexGuard guard(mutex);
            return sizes[index];
         }
         priority& MaxCount(std::size_t MaxCount)
         {
            pc::threads::MutexGuard guard(mutex);

            sizes.resize(MaxCount);
            return *this;
         }
         std::size_t MaxCount() const
         {
            return sizes.size();
         }

         std::size_t MinIdx(int minIdx) const
         {
            for (std::size_t i = 0; i < sizes.size(); ++i)
            {
               // Early return
               // As we know min value
               if (sizes[i] == 0)
                  return i;
               if (sizes[i] < sizes[minIdx])
                  minIdx = i;
            }
            return minIdx;
         }
      };
   } // namespace balancer
} // namespace pc
