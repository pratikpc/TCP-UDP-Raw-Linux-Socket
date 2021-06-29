#pragma once

#include <pthread.h>
#include <pc/network/Mutex.hpp>

namespace pc
{
   namespace network
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
   } // namespace network
} // namespace pc