#pragma once

#include <pthread.h>

namespace pc
{
   namespace network
   {
      class MutexGuard
      {
         pthread_mutex_t& mutex;

       public:
         MutexGuard(pthread_mutex_t& mutex) : mutex(mutex)
         {
            pthread_mutex_lock(&mutex);
         }
         ~MutexGuard()
         {
            pthread_mutex_unlock(&mutex);
         }
      };
   } // namespace network
} // namespace pc