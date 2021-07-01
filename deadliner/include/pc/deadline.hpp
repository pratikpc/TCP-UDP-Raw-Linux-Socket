#pragma once

#include <cstddef>
#include <ctime>

namespace pc
{
   class Deadline
   {
      std::ptrdiff_t maxCount;
      std::ptrdiff_t maxTime;

      bool kill;

      std::vector<std::ptrdiff_t> queue;

      std::ptrdiff_t front;
      std::ptrdiff_t rear;
      timespec       specTime;

    public:
      Deadline(std::size_t maxCount = 25, std::ptrdiff_t maxTime = 10 * 1000) :
          maxCount(maxCount), maxTime(maxTime), kill(false), queue(maxCount), front(-1),
          rear(-1)
      {
      }

      operator bool() const
      {
         return kill;
      }
      std::ptrdiff_t getCurrentTimeMs()
      {
         clock_gettime(CLOCK_MONOTONIC, &specTime);
         return (std::ptrdiff_t)(((std::ptrdiff_t)specTime.tv_sec * 1000) +
                                 specTime.tv_nsec / 1.0e6);
      }

      Deadline& increment()
      {

         std::ptrdiff_t curTime = getCurrentTimeMs();
         // If queue is full
         if ((front == 0 && rear == maxCount - 1) || (front == rear + 1))
         {
            // Compare current time and time at front
            if ((curTime - queue[front]) <= maxTime)
            {
               kill = true;
               return *this;
            }
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

      Deadline& MaxCount(std::size_t MaxCount)
      {
         maxCount = MaxCount;
         return *this;
      }
      std::size_t MaxCount()
      {
         return maxCount;
      }
   };
} // namespace pc