#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

#include <pc/timer/timer.hpp>

#include <pc/queue.hpp>

#ifndef PC_USE_SPINLOCKS
#   include <pc/thread/Mutex.hpp>
#   include <pc/thread/MutexGuard.hpp>
#else
#   include <pc/thread/spin/SpinGuard.hpp>
#   include <pc/thread/spin/SpinLock.hpp>
#endif

#ifndef DEADLINE_MAX_COUNT_DEFAULT
#   ifdef PC_PROFILE
#      define DEADLINE_MAX_COUNT_DEFAULT 10000
// When not testing deadline should be sufficiently low
#   else
#      define DEADLINE_MAX_COUNT_DEFAULT 25
#   endif
#endif

namespace pc
{
   namespace deadliner
   {
      namespace
      {

         std::ptrdiff_t DifferenceInNanos(timespec const& left, timespec const& right)
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

#ifndef PC_USE_SPINLOCKS
         mutable pc::threads::Mutex      lock;
         typedef pc::threads::MutexGuard LockGuard;
#else
         mutable pc::threads::SpinLock  lock;
         typedef pc::threads::SpinGuard LockGuard;
#endif
       public:
         std::ptrdiff_t maxTime;
         std::ptrdiff_t maxHealthCheckTime;

         Deadline(std::size_t    maxCount           = DEADLINE_MAX_COUNT_DEFAULT,
                  std::ptrdiff_t maxTime            = 10 * 1.e9,
                  std::ptrdiff_t maxHealthCheckTime = 30 * 1.e9) :
             queue(maxCount),
             maxTime(maxTime), maxHealthCheckTime(maxHealthCheckTime)
         {
         }

         bool HealthCheckNeeded() const
         {
            timespec curTime = timer::now();

            LockGuard guard(lock);
            if (queue.rear != -1)
            {
               assert(curTime > queue.Last());
               return (DifferenceInNanos(curTime, queue.Last()) > maxHealthCheckTime);
            }
            return false;
         }

         operator bool() const
         {
            timespec curTime = timer::now();

            LockGuard guard(lock);
            // If Full
            if (queue)
            {
               assert(curTime > queue.First());
               return (DifferenceInNanos(curTime, queue.First()) <= maxTime);
            }
            return false;
         }
         Deadline& incrementMaxCount()
         {
            return MaxCount(queue.maxCount + 1);
         }
         Deadline& increment()
         {
            LockGuard guard(lock);

            timespec curTime = timer::now();
            queue.Add(curTime);
            return *this;
         }
         Deadline& operator++()
         {
            return increment();
         }

         Deadline& MaxCount(std::ptrdiff_t newMaxCount)
         {
            LockGuard guard(lock);
            // Check if Queue needs to be expanded
            queue.MaxCount(newMaxCount);
            return *this;
         }
         std::size_t MaxCount() const
         {
            return queue.maxCount;
         }
      };
   } // namespace deadliner
} // namespace pc
