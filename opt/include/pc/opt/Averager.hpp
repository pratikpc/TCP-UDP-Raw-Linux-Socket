#pragma once

#include <cfloat>

#ifdef PC_PROFILE
#   include <ctime>
#   include <ostream>
#endif

namespace pc
{
   namespace opt
   {
      template <typename Numeric = double>
      struct Averager
      {
         Numeric     average;
         Numeric     low;
         Numeric     high;
         std::size_t index;

         Averager(Numeric value = 0, std::size_t index = 0) :
             average(value), low(DBL_MAX), high(DBL_MIN), index(index)
         {
         }
         Averager& operator+=(Numeric const incBy)
         {
            average = ((average * index) + incBy) / (index + 1);
            ++index;
            if (incBy < low)
               low = incBy;
            if (incBy > high)
               high = incBy;
            return *this;
         }
#ifdef PC_PROFILE
         friend std::ostream& operator<<(std::ostream& os, Averager<Numeric> const& value)
         {
            return os << "avg=" << value.average << " low=" << value.low
                      << " high=" << value.high;
         }
         Averager<Numeric>& operator+=(timespec time)
         {
            return (*this += (time.tv_sec * 1.e6 + time.tv_nsec / 1.e3));
         }
#endif
      };
   } // namespace opt
} // namespace pc
