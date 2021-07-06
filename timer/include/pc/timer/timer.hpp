#pragma once
#include <ctime>

bool operator>(timespec const& left, timespec const& right)
{
   return left.tv_sec > right.tv_sec ||
          // If same second, left > right
          (left.tv_sec == right.tv_sec && left.tv_nsec >= right.tv_nsec);
}
bool operator<(timespec const& left, timespec const& right)
{
   return left.tv_sec < right.tv_sec ||
          // If same second, left > right
          (left.tv_sec == right.tv_sec && left.tv_nsec <= right.tv_nsec);
}
namespace pc
{
   namespace timer
   {
      timespec now()
      {
         timespec specTime;
         clock_gettime(CLOCK_MONOTONIC, &specTime);
         return specTime;
      }
      std::size_t millis()
      {
         timespec curTime = now();
         return curTime.tv_sec * 1.e3 + curTime.tv_nsec / 1.e6;
      }

      std::time_t seconds()
      {
         timespec curTime = now();
         return curTime.tv_sec;
      }

   } // namespace timer
} // namespace pc
