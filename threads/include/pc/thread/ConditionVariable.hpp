#pragma once

#include <pthread.h>
#include <stdexcept>

namespace pc
{
   namespace threads
   {
      class ConditionVariable
      {
       public:
         pthread_cond_t condition;

         ConditionVariable()
         {
            if (pthread_cond_init(&condition, NULL) != 0)
            {
               throw std::runtime_error("Mutex init failed");
            }
         }
         template <typename Mutex>
         bool Wait(Mutex& mutex)
         {
            return pthread_cond_wait(&condition, &mutex.mutex) == 0;
         }
         template <typename Mutex>
         bool Wait(Mutex& mutex, timespec const& duration)
         {
            return pthread_cond_timedwait(&condition, &mutex.mutex, &duration) == 0;
         }
         bool Signal()
         {
            return pthread_cond_signal(&condition) == 0;
         }
         bool Broadcast()
         {
            return pthread_cond_broadcast(&condition) == 0;
         }

         ~ConditionVariable()
         {
            pthread_cond_destroy(&condition);
         }
      };
   } // namespace threads
} // namespace pc
