#pragma once

#ifndef PC_USE_SPINLOCKS
#   include <pc/thread/Mutex.hpp>
#   include <pc/thread/MutexGuard.hpp>
#else
#   include <pc/thread/spin/SpinGuard.hpp>
#   include <pc/thread/spin/SpinLock.hpp>
#endif

#include <ostream>
#include <vector>

namespace pc
{
   namespace balancer
   {
      class priority
      {
         std::vector<std::size_t> sizes;
#ifndef PC_USE_SPINLOCKS
         mutable pc::threads::Mutex      lock;
         typedef pc::threads::MutexGuard LockGuard;
#else
         mutable pc::threads::SpinLock  lock;
         typedef pc::threads::SpinGuard LockGuard;
#endif
         std::size_t lowestSizeIndex;

       public:
         friend std::ostream& operator<<(std::ostream& os, priority& queue)
         {
            LockGuard guard(queue.lock);
            for (std::size_t i = 0; i < queue.sizes.size(); ++i)
               os << queue.sizes[i] << " : ";
            return os;
         }
         priority(std::size_t maxSize) : sizes(maxSize), lowestSizeIndex(0) {}

         std::size_t operator*() const
         {
            LockGuard guard(lock);
            return lowestSizeIndex;
         }
         void incPriority(std::size_t index, std::size_t by = 1)
         {
            LockGuard guard(lock);
            sizes[index] += by;
            lowestSizeIndex = MinIdx(lowestSizeIndex);
         }
         void decPriority(std::size_t index, std::size_t by = 1)
         {
            LockGuard guard(lock);
            if (sizes[index] >= by)
            {
               sizes[index] -= by;
               lowestSizeIndex = MinIdx(lowestSizeIndex);
            }
         }
         void setPriority(std::size_t index, std::size_t value)
         {
            LockGuard guard(lock);
            sizes[index]    = value;
            lowestSizeIndex = MinIdx(lowestSizeIndex);
         }
         std::size_t operator[](std::size_t index) const
         {
            LockGuard guard(lock);
            return sizes[index];
         }
         priority& MaxCount(std::size_t MaxCount)
         {
            LockGuard guard(lock);

            sizes.resize(MaxCount);
            return *this;
         }
         std::size_t MaxCount() const
         {
            LockGuard guard(lock);
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
