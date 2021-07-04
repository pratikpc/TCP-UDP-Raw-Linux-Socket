#pragma once

#include <vector>

namespace pc
{
   namespace opt
   {
      template <typename T>
      class Vector
      {
         std::vector<T> data;

         bool hasValue;

       public:
         Vector(std::size_t size = 0) : data(size), hasValue(false) {}

         operator bool() const
         {
            return !data.empty() && hasValue;
         }
         operator std::size_t() const
         {
            return data.size();
         }
         Vector<T> operator=(bool const value)
         {
            hasValue = value;
            return *this;
         }
         std::vector<T>* operator->() const
         {
            return &data;
         }
         std::size_t size()
         {
            return data.size();
         }
         std::vector<T>* operator->()
         {
            return &data;
         }
         operator T*()
         {
            return data.data();
         }
         operator T*() const
         {
            return data.data();
         }
      };
   } // namespace opt
} // namespace pc
