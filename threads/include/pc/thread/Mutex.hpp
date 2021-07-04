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
         ~Mutex()
         {
            pthread_mutex_destroy(&mutex);
         }
      };
   } // namespace threads
} // namespace pc
