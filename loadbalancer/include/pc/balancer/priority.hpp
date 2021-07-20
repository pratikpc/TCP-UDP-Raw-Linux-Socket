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
         typedef std::vector<std::size_t> SizeVec;
         typedef SizeVec::const_iterator  const_iterator;

         SizeVec sizes;
#ifndef PC_USE_SPINLOCKS
         mutable pc::threads::Mutex      lock;
         typedef pc::threads::MutexGuard LockGuard;
#else
         mutable pc::threads::SpinLock  lock;
         typedef pc::threads::SpinGuard LockGuard;
#endif
         std::size_t minIdx;
         std::size_t maxIdx;

       public:
         friend std::ostream& operator<<(std::ostream& os, priority& queue)
         {
            LockGuard guard(queue.lock);
            for (const_iterator i = queue.sizes.begin(); i != queue.sizes.end(); ++i)
               os << *i << " : ";
            return os;
         }
         priority(std::size_t maxSize) : sizes(maxSize), minIdx(0), maxIdx(0) {}

         std::size_t operator*() const
         {
            return NextIndex();
         }
         std::size_t NextIndex() const
         {
            LockGuard guard(lock);
            return minIdx;
         }
         std::size_t MaxIndex() const
         {
            LockGuard guard(lock);
            return maxIdx;
         }
         void incPriority(std::size_t index, std::size_t by = 1)
         {
            LockGuard guard(lock);
            sizes[index] += by;
            SetMinMaxIdx();
         }
         void decPriority(std::size_t index, std::size_t by = 1)
         {
            LockGuard guard(lock);
            if (sizes[index] >= by)
            {
               sizes[index] -= by;
               SetMinMaxIdx();
            }
         }
         void setPriority(std::size_t index, std::size_t value)
         {
            LockGuard guard(lock);
            sizes[index] = value;
            SetMinMaxIdx();
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

         void SetMinMaxIdx()
         {
            for (std::size_t i = 0; i < sizes.size(); ++i)
            {
               // Early return
               // As we know min value
               if (sizes[i] == 0 || sizes[i] < sizes[minIdx])
                  minIdx = i;
               if (sizes[i] > sizes[maxIdx])
                  maxIdx = i;
            }
         }
      };
   } // namespace balancer
} // namespace pc
