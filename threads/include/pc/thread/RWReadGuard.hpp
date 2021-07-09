#pragma once

#include <pc/thread/RWLock.hpp>
#include <pthread.h>

namespace pc
{
   namespace threads
   {
      class RWReadGuard
      {
         pthread_rwlock_t& mutex;

       public:
         RWReadGuard(RWLock& p_mutex) : mutex(p_mutex.rwLock)
         {
            pthread_rwlock_rdlock(&mutex);
         }
         ~RWReadGuard()
         {
            pthread_rwlock_unlock(&mutex);
         }
      };
   } // namespace threads
} // namespace pc
