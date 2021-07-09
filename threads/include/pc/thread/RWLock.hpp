#pragma once

#include <pthread.h>
#include <stdexcept>

namespace pc
{
   namespace threads
   {
      class RWLock
      {
       public:
         pthread_rwlock_t rwLock;

         RWLock()
         {
            if (pthread_rwlock_init(&rwLock, NULL) != 0)
            {
               throw std::runtime_error("Mutex init failed");
            }
         }
         ~RWLock()
         {
            pthread_rwlock_destroy(&rwLock);
         }
      };
   } // namespace threads
} // namespace pc
