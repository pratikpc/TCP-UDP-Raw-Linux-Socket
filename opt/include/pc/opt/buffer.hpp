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
         Buffer(std::size_t size) : arr(new T[size]), sizeV(size) {}
         ~Buffer()
         {
            if (arr != NULL)
               delete[] arr;
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
