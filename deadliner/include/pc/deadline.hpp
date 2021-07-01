#pragma once

#include <cstddef>
#include <ctime>
#include <vector>

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

namespace pc
{
   class Deadline
   {
      std::ptrdiff_t maxCount;
      std::ptrdiff_t maxTime;

      std::vector<std::ptrdiff_t> queue;

      std::ptrdiff_t front;
      std::ptrdiff_t rear;

      mutable pc::threads::Mutex mutex;

    public:
      Deadline(std::size_t maxCount = 25, std::ptrdiff_t maxTime = 10 * 1000) :
          maxCount(maxCount), maxTime(maxTime), queue(maxCount), front(-1), rear(-1)
      {
      }

      operator bool() const
      {
         std::ptrdiff_t          curTime = getCurrentTimeMs();
         pc::threads::MutexGuard guard(mutex);
         if ((front == 0 && rear == maxCount - 1) || (front == rear + 1))
            return ((curTime - queue[front]) <= maxTime);
         return false;
      }
      static std::ptrdiff_t getCurrentTimeMs()
      {
         timespec specTime;
         clock_gettime(CLOCK_MONOTONIC, &specTime);
         return (std::ptrdiff_t)(((std::ptrdiff_t)specTime.tv_sec * 1000) +
                                 specTime.tv_nsec / 1.0e6);
      }

      Deadline& increment()
      {
         pc::threads::MutexGuard guard(mutex);
         std::ptrdiff_t          curTime = getCurrentTimeMs();
         // If queue is full
         if ((front == 0 && rear == maxCount - 1) || (front == rear + 1))
         {
            // Deque current value
            front = (front + 1) % maxCount;
         }
         if (front == -1)
            front = 0;
         rear        = (rear + 1) % maxCount;
         queue[rear] = curTime;
         return *this;
      }
      Deadline& operator++()
      {
         return increment();
      }

      Deadline& MaxCount(std::ptrdiff_t newMaxCount)
      {
         pc::threads::MutexGuard guard(mutex);
         // Check if Queue needs to be expanded
         if (maxCount < newMaxCount)
         {
            if (front > rear)
            {
               // Copy from start to finish to temporary
               std::vector<std::ptrdiff_t> temp(newMaxCount);
               for (std::ptrdiff_t i = front, j = 0; i != rear;
                    i = (i + 1) % maxCount, ++j)
                  temp[j] = queue[i];
               // Set front and rear to 0 and new end
               front = 0;
               rear  = maxCount;
               // Clear queue
               queue.clear();
               // Move temp to queue
               queue = temp;
            }
            // If the array is not yet circular
            // Simply resize
            else
               queue.resize(newMaxCount);
         }
         maxCount = newMaxCount;
         return *this;
      }
      std::size_t MaxCount() const
      {
         return maxCount;
      }
   };
} // namespace pc