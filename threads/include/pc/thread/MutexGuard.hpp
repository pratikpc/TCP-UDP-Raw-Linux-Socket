#pragma once

#include <pc/thread/Mutex.hpp>
#include <pthread.h>

namespace pc
{
   namespace threads
   {
      class MutexGuard
      {
         pthread_mutex_t& mutex;

       public:
         MutexGuard(Mutex& p_mutex) : mutex(p_mutex.mutex)
         {
#ifndef PC_GAIN_SPEED_BY_GUARD_DISABLE
            pthread_mutex_lock(&mutex);
#endif
         }
         ~MutexGuard()
         {
#ifndef PC_GAIN_SPEED_BY_GUARD_DISABLE
            pthread_mutex_unlock(&mutex);
#endif
         }
      };
   } // namespace threads
} // namespace pc
