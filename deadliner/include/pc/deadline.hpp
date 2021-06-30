#pragma once

#include <cstddef>
#include <ctime>

namespace pc
{
   class Deadline
   {
      std::size_t maxCount;
      std::time_t maxTime;

      std::size_t curSize;
      std::time_t timeInSeconds;
      bool        kill;

      std::vector<time_t> queue;

      std::ptrdiff_t front;
      std::ptrdiff_t rear;
      timespec       specTime;

    public:
      Deadline(std::size_t maxCount = 25, std::size_t maxTime = 15*1000) :
          maxCount(maxCount), maxTime(maxTime), curSize(0), kill(false), queue(maxCount),
          front(-1), rear(-1)
      {
      }

      operator bool() const
      {
         return kill;
      }
      time_t getCurrentTimeMs()
      {
         clock_gettime(CLOCK_MONOTONIC, &specTime);
         return specTime.tv_nsec / 1.0e6;
      }

      Deadline& operator++()
      {
         time_t curTime = getCurrentTimeMs();
         if ((front == 0 && rear == maxCount - 1) || (front == rear + 1))
         {
            if ((curTime - queue[front]) <= maxTime)
               kill = true;
            else
               front = (front + 1) % maxCount;
         }
         if (front == -1)
            front = 0;
         rear        = (rear + 1) % maxCount;
         queue[rear] = curTime;
         return *this;
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