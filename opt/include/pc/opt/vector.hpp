#pragma once

#include <vector>

namespace pc
{
   namespace opt
   {
      template <typename T>
      class Vector
      {
         typedef std::vector<T> Items;

         Items items;

       public:
         bool hasValue;

       public:
         Vector(std::size_t size = 0) : items(size), hasValue(false) {}

         operator bool() const
         {
            return !items.empty() && hasValue;
         }

         operator std::size_t() const
         {
            return items.size();
         }
         Vector<T>& setIfHasValue(bool const value)
         {
            hasValue = value;
            return *this;
         }
         Vector<T>& setHasValue()
         {
            return setIfHasValue(true);
         }
         Vector<T>& setDoesNotHaveValue()
         {
            return setIfHasValue(false);
         }
         std::vector<T>* operator->()
         {
            return &items;
         }
         std::size_t size()
         {
            return items.size();
         }
         T* data()
         {
            return items.data();
         }
         const T* data() const
         {
            return items.data();
         }
         T& operator[](std::size_t index)
         {
            return items[index];
         }
         T operator[](std::size_t index) const
         {
            return items[index];
         }
         typename Items::iterator begin()
         {
            return items.begin();
         }
         typename Items::iterator end()
         {
            return items.end();
         }
         typename Items::const_iterator begin() const
         {
            return items.begin();
         }
         typename Items::const_iterator end() const
         {
            return items.end();
         }
      };
   } // namespace opt
} // namespace pc
