#pragma once

#include <cstddef>

namespace pc
{
   namespace memory
   {
      template <typename T>
      class unique_ptr
      {
       public:
         T* data;

         unique_ptr() : data(new T) {}
         unique_ptr(unique_ptr& other) : data(other.data)
         {
            other.data = NULL;
         }
         T& operator*()
         {
            return *data;
         }
         T const& operator*() const
         {
            return *data;
         }
         T const* operator->() const
         {
            return data;
         }
         T* operator->()
         {
            return data;
         }
         operator bool() const
         {
            return (data != NULL);
         }
         ~unique_ptr()
         {
            if (*this)
               delete data;
         }
      };
   } // namespace memory
} // namespace pc