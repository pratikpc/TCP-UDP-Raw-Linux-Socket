#pragma once

#include <pc/thread/spin/SpinGuard.hpp>
#include <pc/thread/spin/SpinLock.hpp>

namespace pc
{
   namespace threads
   {
      // We need C++11
      // for Real Atomic Bool
      template <typename T>
      class Atomic
      {
       protected:
         mutable pc::threads::SpinLock mutex;

         T value;

       public:
         Atomic(T value = T()) : value(value) {}
         operator T() const
         {
            pc::threads::SpinLock guard(mutex);
            return value;
         }
         Atomic& operator=(T const newValue)
         {
            pc::threads::SpinLock guard(mutex);
            value = newValue;
            return *this;
         }
      };
   } // namespace threads
} // namespace pc
