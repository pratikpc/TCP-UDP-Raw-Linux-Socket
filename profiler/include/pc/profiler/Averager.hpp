#pragma once

#include <cfloat>

#ifdef PC_PROFILE
#   include <ctime>
#   include <ostream>
#endif

namespace pc
{
   namespace profiler
   {
      template <typename Numeric = double>
      struct Averager
      {
         Numeric     average;
         Numeric     low;
         Numeric     high;
         std::size_t count;
         std::size_t NoOfClosesToHigh;
         Averager(Numeric value = 0, std::size_t count = 0) :
             average(value), low(DBL_MAX), high(DBL_MIN), count(count),
             NoOfClosesToHigh(0)
         {
         }
         Averager& operator+=(Numeric const incBy)
         {
            average = ((average * count) + incBy) / (count + 1);
            ++count;
            if (incBy < low)
               low = incBy;
            if (incBy > high)
               high = incBy;
            if (incBy >= 0.2 * high)
               ++NoOfClosesToHigh;
            return *this;
         }
#ifdef PC_PROFILE
         friend std::ostream& operator<<(std::ostream& os, Averager<Numeric> const& value)
         {
            return os << std::setprecision(4) << "avg=" << value.average
                      << " low=" << value.low << " high=" << value.high << " closesHigh="
                      << (((value.NoOfClosesToHigh * 1.0) / value.count) * 100.0)
                      << " closeCount=" << value.NoOfClosesToHigh
                      << " totalC=" << value.count;
         }
         Averager<Numeric>& operator+=(timespec time)
         {
            return (*this += (time.tv_sec * 1.e6 + time.tv_nsec / 1.e3));
         }
#endif
      };
   } // namespace opt
} // namespace pc
