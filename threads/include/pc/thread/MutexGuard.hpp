#pragma once

#include <pc/thread/GenericMutexGuard.hpp>
#include <pc/thread/Mutex.hpp>

namespace pc
{
   namespace threads
   {
      typedef GenericMutexGuard<Mutex> MutexGuard;
   } // namespace threads
} // namespace pc
