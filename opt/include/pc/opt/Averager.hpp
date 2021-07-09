#pragma once

#include <cstddef>

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
         operator T() const
         {
            return value;
         }
         T val() const
         {
            T const value = *this;
            return value;
         }
      };
   } // namespace opt
} // namespace pc
