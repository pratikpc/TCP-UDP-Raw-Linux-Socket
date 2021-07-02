#pragma once

#include <cassert>
#include <cstddef>
#include <ctime>
#include <vector>

#include <pc/queue.hpp>

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
    protected:
      pc::queue<timespec> queue;

      std::ptrdiff_t maxTime;
      std::ptrdiff_t maxHealthCheckTime;

      mutable pc::threads::Mutex mutex;

    public:
      Deadline(std::size_t    maxCount           = 25,
               std::ptrdiff_t maxTime            = 10 * 1.e9,
               std::ptrdiff_t maxHealthCheckTime = 100 * 1.e9) :
          queue(maxCount),
          maxTime(maxTime), maxHealthCheckTime(maxHealthCheckTime)
      {
      }

      bool HealthCheckNeeded() const
      {
         timespec curTime = getCurrentTime();

         pc::threads::MutexGuard guard(mutex);
         if (queue.front != -1)
         {
            assert(curTime > queue.Last());
            return ((curTime - queue.Last()) > maxTime);
         }
         return false;
      }

      operator bool() const
      {
         timespec curTime = getCurrentTime();

         pc::threads::MutexGuard guard(mutex);
         // If Full
         if (queue)
         {
            assert(curTime > queue.First());
            return ((curTime - queue.First()) <= maxTime);
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
         return MaxCount(queue.maxCount + 1);
      }
      Deadline& increment()
      {
         pc::threads::MutexGuard guard(mutex);

         timespec curTime = getCurrentTime();
         queue.Add(curTime);
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
         queue.MaxCount(newMaxCount);
         return *this;
      }
      std::size_t MaxCount() const
      {
         return queue.maxCount;
      }
   };
} // namespace pc