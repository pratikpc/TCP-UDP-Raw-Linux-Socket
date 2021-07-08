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
      class AtomicBool
      {
         mutable pc::threads::Mutex mutex;

         bool value;

       public:
         AtomicBool(bool value = false) : value(value) {}
         operator bool() const
         {
            pc::threads::MutexGuard guard(mutex);
            return value;
         }
         AtomicBool& operator=(bool const newValue)
         {
            pc::threads::MutexGuard guard(mutex);
            value = newValue;
            return *this;
         }
      };
   } // namespace threads
} // namespace pc
