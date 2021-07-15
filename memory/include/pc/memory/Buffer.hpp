#pragma once

#include <cassert>

namespace pc
{
   namespace memory
   {
      template <typename T>
      class Buffer
      {
         T* const    arr;
         std::size_t sizeV;
         std::size_t offset;

       public:
         Buffer(std::size_t size) : arr(new T[size]), sizeV(size), offset(0)
         {
            assert(size > 0);
         }
         ~Buffer()
         {
            if (arr != NULL)
               delete[] arr;
         }

         void Offset(std::size_t newOffset)
         {
            assert(offset < sizeV);
            offset = newOffset;
         }
         std::size_t Offset() const
         {
            return offset;
         }

         std::size_t size() const
         {
            return sizeV - offset;
         }
         T* data()
         {
            return arr + offset;
         }
         const T* data() const
         {
            return arr + offset;
         }
         T& operator[](std::size_t index)
         {
            return arr[index + offset];
         }
         T operator[](std::size_t index) const
         {
            return arr[index + offset];
         }
         T* begin() const
         {
            return arr + offset;
         }
      };
   } // namespace memory
} // namespace pc
