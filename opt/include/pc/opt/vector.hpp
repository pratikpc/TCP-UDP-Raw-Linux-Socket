#pragma once

#include <vector>

namespace pc
{
   namespace opt
   {
      template <typename T>
      class Vector
      {
         typedef std::vector<T> Data;

         Data data;
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
         std::vector<T>* operator->()
         {
            return &data;
         }
         std::size_t size()
         {
            return data.size();
         }
         operator T*()
         {
            return data.data();
         }
         operator T*() const
         {
            return data.data();
         }
         T& operator[](std::size_t index)
         {
            return data[index];
         }
         T operator[](std::size_t index) const
         {
            return data[index];
         }
         typename Data::iterator begin()
         {
            return data.begin();
         }
         typename Data::iterator end()
         {
            return data.end();
         }
         typename Data::const_iterator begin() const
         {
            return data.begin();
         }
         typename Data::const_iterator end() const
         {
            return data.end();
         }
      };
   } // namespace opt
} // namespace pc
