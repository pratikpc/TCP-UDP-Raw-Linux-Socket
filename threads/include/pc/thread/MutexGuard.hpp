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
            pthread_mutex_lock(&mutex);
         }
         ~MutexGuard()
         {
            pthread_mutex_unlock(&mutex);
         }
      };
   } // namespace threads
} // namespace pc
