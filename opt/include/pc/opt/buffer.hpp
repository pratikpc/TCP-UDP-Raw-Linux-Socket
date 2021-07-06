#pragma once

#include <vector>

namespace pc
{
   namespace opt
   {
      template <typename T>
      class Buffer
      {
         T*          arr;
         std::size_t sizeV;

       public:
         bool hasValue;

       public:
         Buffer(std::size_t size = 0) : arr(new T[size]), sizeV(size), hasValue(false) {}
         ~Buffer()
         {
            if (arr != NULL)
               delete[] arr;
         }
         operator bool() const
         {
            return arr != NULL && hasValue;
         }

         operator std::size_t() const
         {
            return size();
         }
         Buffer<T>& setIfHasValue(bool const value)
         {
            hasValue = value;
            return *this;
         }
         Buffer<T>& setHasValue()
         {
            return setIfHasValue(true);
         }
         Buffer<T>& setDoesNotHaveValue()
         {
            return setIfHasValue(false);
         }
         std::size_t size() const
         {
            return sizeV;
         }
         T* data()
         {
            return arr;
         }
         const T* data() const
         {
            return arr;
         }
         T& operator[](std::size_t index)
         {
            return arr[index];
         }
         T operator[](std::size_t index) const
         {
            return arr[index];
         }
         T* begin() const
         {
            return arr;
         }
      };
   } // namespace opt
} // namespace pc
