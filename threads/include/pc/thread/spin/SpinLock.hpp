#pragma once

#include <pthread.h>
#include <stdexcept>

namespace pc
{
   namespace threads
   {
      class SpinLock
      {
         pthread_spinlock_t spinlock;

       public:
         SpinLock(int const pshared = PTHREAD_PROCESS_PRIVATE)
         {
            if (pthread_spin_init(&spinlock, pshared) != 0)
            {
               throw std::runtime_error("Mutex init failed");
            }
         }
         void Lock()
         {
#ifndef PC_GAIN_SPEED_BY_GUARD_DISABLE
            pthread_spin_lock(&spinlock);
#endif
         }
         void Unlock()
         {
#ifndef PC_GAIN_SPEED_BY_GUARD_DISABLE
            pthread_spin_unlock(&spinlock);
#endif
         }
         ~SpinLock()
         {
            pthread_spin_destroy(&spinlock);
         }
      };
   } // namespace threads
} // namespace pc
