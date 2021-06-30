#pragma once

#include <cstddef>

namespace pc
{
   namespace memory
   {
      template <typename T>
      class unique_arr
      {
       public:
         std::size_t size;
         T*          data;

         unique_arr(std::size_t size) : size(size), data(new T[size]) {}
         unique_arr() : size(0), data(NULL) {}
         unique_arr(unique_arr& other) : size(other.size), data(other.data)
         {
            other.data = NULL;
         }
         unique_arr& operator=(unique_arr& other)
         {
            data       = other.data;
            size       = other.size;
            other.data = NULL;
         }
         T* get() const
         {
            return data;
         }
         T& operator[](std::size_t i)
         {
            return get()[i];
         }
         operator bool() const
         {
            return (data != NULL);
         }
         ~unique_arr()
         {
            if (data != NULL)
            {
               delete[] data;
            }
         }
      };
   } // namespace memory
} // namespace pc