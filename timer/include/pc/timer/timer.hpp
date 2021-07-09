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
#ifdef PC_PROFILE
timespec operator-(timespec const& a, timespec const& b)
{
   timespec result;
   result.tv_sec  = a.tv_sec - b.tv_sec;
   result.tv_nsec = a.tv_nsec - b.tv_nsec;
   if (result.tv_nsec < 0)
   {
      --result.tv_sec;
      result.tv_nsec += 1000000000L;
   }
   return result;
}
#   include <ostream>
std::ostream& operator<<(std::ostream& os, timespec const& display)
{
   return os << display.tv_sec << " sec " << (display.tv_nsec) / (1.e3) << " usec";
}
#endif

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
