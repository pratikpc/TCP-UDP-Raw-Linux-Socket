#pragma once

#include <pthread.h>
#include <stdexcept>

namespace pc
{
   namespace threads
   {
      class Mutex
      {
       public:
         pthread_mutex_t mutex;

         Mutex()
         {
            if (pthread_mutex_init(&mutex, NULL) != 0)
            {
               throw std::runtime_error("Mutex init failed");
            }
         }
         void Lock()
         {
#ifndef PC_GAIN_SPEED_BY_GUARD_DISABLE
            pthread_mutex_lock(&mutex);
#endif
         }
         void Unlock()
         {
#ifndef PC_GAIN_SPEED_BY_GUARD_DISABLE
            pthread_mutex_unlock(&mutex);
#endif
         }
         ~Mutex()
         {
            pthread_mutex_destroy(&mutex);
         }
      };
   } // namespace threads
} // namespace pc
