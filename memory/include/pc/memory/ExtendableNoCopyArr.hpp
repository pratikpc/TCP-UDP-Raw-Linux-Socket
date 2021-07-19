#pragma once

#include <cassert>

namespace pc
{
   namespace memory
   {
      template <typename T>
      class ExtendableNoCopyArr
      {
       protected:
         T mutable*  arr;
         std::size_t sizeV;

       public:
         ExtendableNoCopyArr(std::size_t size = 2) : arr(new T[size]), sizeV(size)
         {
            assert(size > 0);
         }
         ExtendableNoCopyArr(ExtendableNoCopyArr const& other) :
             arr(other.arr), sizeV(other.sizeV)
         {
            other.arr = NULL;
         }
         ~ExtendableNoCopyArr()
         {
            delete[] arr;
            arr = NULL;
         }

         void ExtendTo(std::size_t newSize)
         {
            // Only extend
            if (newSize <= sizeV)
               return;
            sizeV = newSize;
            // Upon extend just delete the old memory
            // Data is not of value here
            if (arr != NULL)
               delete[] arr;
            arr = new T[sizeV];
         }
         void ExtendBy(std::ptrdiff_t by)
         {
            if (by > 0)
               return ExtendTo(sizeV + by);
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
         const T* begin() const
         {
            return arr;
         }
         const T* end() const
         {
            return arr + sizeV;
         }
      };
   } // namespace memory
} // namespace pc
