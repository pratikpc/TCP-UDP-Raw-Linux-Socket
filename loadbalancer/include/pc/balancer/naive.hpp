#pragma once

#include <cstddef>

namespace pc
{
   namespace balancer
   {
      class naive
      {
         std::size_t maxCount;
         std::size_t curSize;

       public:
         naive(std::size_t maxCount) : maxCount(maxCount), curSize(0) {}

         std::size_t operator*() const
         {
            return curSize;
         }
         naive& operator++()
         {
            curSize = (curSize + 1) % maxCount;
            return *this;
         }
         naive& MaxCount(std::size_t MaxCount)
         {
            maxCount = MaxCount;
            return *this;
         }
      };
   } // namespace balancer
} // namespace pc