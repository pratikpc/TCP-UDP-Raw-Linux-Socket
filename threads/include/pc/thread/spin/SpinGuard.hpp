#pragma once

#include <pc/thread/GenericMutexGuard.hpp>
#include <pc/thread/spin/SpinLock.hpp>

namespace pc
{
   namespace threads
   {
      typedef GenericMutexGuard<SpinLock> SpinGuard;
   } // namespace threads
} // namespace pc
