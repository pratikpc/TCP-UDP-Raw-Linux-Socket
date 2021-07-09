#pragma once

#include <pc/thread/RWLock.hpp>
#include <pthread.h>

namespace pc
{
   namespace threads
   {
      class RWWriteGuard
      {
         pthread_rwlock_t& mutex;

       public:
         RWWriteGuard(RWLock& p_mutex) : mutex(p_mutex.rwLock)
         {
            pthread_rwlock_rdlock(&mutex);
         }
         ~RWWriteGuard()
         {
            pthread_rwlock_unlock(&mutex);
         }
      };
   } // namespace threads
} // namespace pc
