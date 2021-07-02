#pragma once

#include <cassert>
#include <cstddef>
#include <ctime>
#include <vector>

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

namespace pc
{
   namespace
   {
      bool operator>(timespec const& left, timespec const& right)
      {
         return left.tv_sec > right.tv_sec ||
                // If same second, left > right
                (left.tv_sec == right.tv_sec && left.tv_nsec >= right.tv_nsec);
      }
      std::ptrdiff_t operator-(timespec const& left, timespec const& right)
      {
         timespec ret;
         // Note that in our case left sec > right sec always
         ret.tv_sec = left.tv_sec - right.tv_sec;
         // If Left and right seconds are different
         // tv_nsec will always be positive
         // But if left_sec = right_sec
         //    Then it can be negative
         // As a result, nanos would be negative
         ret.tv_nsec = left.tv_nsec - right.tv_nsec;
         // But fret not.
         // If seconds are equal, nanos are positive
         // Ensuring our result is always positive
         // If seconds are not equal, it means the difference in seconds is positive
         // But negative nanos are handled because seconds
         // are converted to nanos
         // And seconds to nanos >= nanos themselves
         // Again >= 0 solution
         // Sample
         // 2 minute 45 - 1 minute 55
         // Under our algo
         // 1 minute -10 seconds
         // That looks weird. But we will convert it to seconds
         // But 60 seconds - 10 seconds
         // Is 50 seconds so handled
         return ((std::ptrdiff_t)ret.tv_sec) * 1.e9 + ret.tv_nsec;
      }
   } // namespace

   class Deadline
   {
      typedef std::vector<timespec> timeQueue;

    protected:
      std::ptrdiff_t maxCount;
      std::ptrdiff_t maxTime;
      timeQueue      queue;
      std::ptrdiff_t front;
      std::ptrdiff_t rear;

      mutable pc::threads::Mutex mutex;

    public:
      Deadline(std::size_t maxCount = 25, std::ptrdiff_t maxTime = 10 * 1.e9) :
          maxCount(maxCount), maxTime(maxTime), queue(maxCount), front(-1), rear(-1)
      {
      }

      operator bool() const
      {
         timespec curTime = getCurrentTime();

         pc::threads::MutexGuard guard(mutex);
         if ((front == 0 && rear == maxCount - 1) || (front == rear + 1))
         {
            assert(curTime > queue[front]);
            return ((curTime - queue[front]) <= maxTime);
         }
         return false;
      }
      static timespec getCurrentTime()
      {
         timespec specTime;
         clock_gettime(CLOCK_MONOTONIC, &specTime);
         return specTime;
      }
      Deadline& incrementMaxCount()
      {
         return MaxCount(maxCount + 1);
      }
      Deadline& increment()
      {
         pc::threads::MutexGuard guard(mutex);

         timespec curTime = getCurrentTime();
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
               timeQueue temp(newMaxCount);
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