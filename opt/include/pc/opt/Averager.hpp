#pragma once

#ifdef PC_PROFILE
#   include <ctime>
#endif

namespace pc
{
   namespace opt
   {
      template <typename T = double>
      struct Averager
      {
         T           value;
         std::size_t index;
         Averager(T value = 0, std::size_t index = 0) : value(value), index(index) {}
         Averager& operator+=(T const incBy)
         {
            value = ((value * index) + incBy) / (index + 1);
            ++index;
            return *this;
         }
#ifdef PC_PROFILE
         operator T() const
         {
            return value;
         }
         Averager<Numeric>& operator+=(timespec time)
         {
            return (*this += (time.tv_sec * 1.e6 + time.tv_nsec / 1.e3));
         }
#endif
         }
      };
   } // namespace opt
} // namespace pc
