#pragma once

#include <cstddef>
#include <ctime>

namespace pc
{
   class Deadline
   {
      std::size_t maxCount;
      std::size_t curSize;
      std::time_t timeInSeconds;
      bool        kill;

    public:
      Deadline(std::size_t maxCount = 20) : maxCount(maxCount), curSize(0), kill(false) {}

      operator bool() const
      {
         return kill;
      }
      time_t getCurrentTime()
      {
         return time(NULL);
      }

      Deadline& operator++()
      {
         ++curSize;
         time_t curTime = getCurrentTime();
         if (curTime == timeInSeconds)
         {
            kill = (curSize >= maxCount);
         }
         else
         {
            curSize       = 0;
            timeInSeconds = curTime;
         }
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