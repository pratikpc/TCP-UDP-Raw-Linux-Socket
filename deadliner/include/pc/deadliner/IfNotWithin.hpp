#pragma once

#include <pc/timer/timer.hpp>

namespace pc
{
   namespace deadliner
   {
      class IfNotWithin
      {
         std::time_t lastDeadlineCheck;

       public:
         std::time_t maxDurationDifference;

         IfNotWithin(std::time_t maxDurationDifferenceSeconds = 10) :
             lastDeadlineCheck(timer::seconds()),
             maxDurationDifference(maxDurationDifferenceSeconds)
         {
         }
         operator bool()
         {
            std::time_t const currentTimeSc = timer::seconds();
            if ((currentTimeSc - lastDeadlineCheck) > maxDurationDifference)
            {
               lastDeadlineCheck = currentTimeSc;
               return true;
            }
            return false;
         }
      };
   } // namespace deadliner
} // namespace pc
