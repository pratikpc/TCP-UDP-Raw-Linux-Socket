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
         T*          arr;
         std::size_t sizeV;

       public:
         ExtendableNoCopyArr(std::size_t size) : arr(new T[size]), sizeV(size), offset(0)
         {
            assert(size > 0);
         }
         ~ExtendableNoCopyArr()
         {
            delete[] arr;
         }

         void ExtendTo(std::size_t newSize)
         {
            // Only extend
            if (newSize <= sizeV)
               return;
            sizeV = newSize;
            // Upon extend just delete the old memory
            // Data is not of value here
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
         T* begin() const
         {
            return arr;
         }
      };
   } // namespace memory
} // namespace pc
