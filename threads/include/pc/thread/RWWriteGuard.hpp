#pragma once

#include <cassert>
#include <pc/thread/RWLock.hpp>
#include <pthread.h>
namespace pc
{
   namespace threads
   {
      class RWWriteGuard
      {
         pthread_rwlock_t& rwLock;
         bool const        locked;

       public:
         RWWriteGuard(RWLock& rwLock) :
             rwLock(rwLock.rwLock), locked(pthread_rwlock_wrlock(&rwLock.rwLock) == 0)
         {
            assert(locked);
         }
         ~RWWriteGuard()
         {
            if (locked)
               pthread_rwlock_unlock(&rwLock);
         }
      };
   } // namespace threads
} // namespace pc
