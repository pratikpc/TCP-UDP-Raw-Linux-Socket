#pragma once

#include <ctime>

namespace pc
{
   namespace deadliner
   {
      class IfNotWithin
      {
         std::time_t lastDeadlineCheck;
         std::time_t maxDurationDifference;

       public:
         IfNotWithin(std::time_t maxDurationDifferenceSeconds = 10) :
             lastDeadlineCheck(std::time(NULL)),
             maxDurationDifference(maxDurationDifferenceSeconds)
         {
         }
         operator bool()
         {
            std::time_t currentTimeSc = std::time(NULL);
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
