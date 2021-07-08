#pragma once

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

#include <pthread.h>

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
         mutable pc::threads::Mutex mutex;

         T value;

       public:
         Atomic(T value = T()) : value(value) {}
         operator T() const
         {
            pc::threads::MutexGuard guard(mutex);
            return value;
         }
         Atomic& operator=(T const newValue)
         {
            pc::threads::MutexGuard guard(mutex);
            value = newValue;
            return *this;
         }
      };
   } // namespace threads
} // namespace pc
