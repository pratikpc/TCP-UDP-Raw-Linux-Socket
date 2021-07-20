#pragma once

#include <pc/thread/RWLock.hpp>
#include <pthread.h>

namespace pc
{
   namespace threads
   {
      class RWReadGuard
      {
         pthread_rwlock_t& rwLock;
         bool const        locked;

       public:
         RWReadGuard(RWLock& rwLock) :
             rwLock(rwLock.rwLock), locked(pthread_rwlock_rdlock(&rwLock.rwLock) == 0)
         {
         }
         ~RWReadGuard()
         {
            if (locked)
               pthread_rwlock_unlock(&rwLock);
         }
      };
   } // namespace threads
} // namespace pc
