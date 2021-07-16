#pragma once

#include <semaphore.h>
#include <stdexcept>

namespace pc
{
   namespace threads
   {
      class Semaphore
      {
       public:
         sem_t semaphore;

         Semaphore(int          pshared      = 0 /*Shared between threads*/,
                   unsigned int initialValue = 0)
         {
            if (sem_init(&semaphore, pshared, initialValue) != 0)
               throw std::runtime_error("Semaphore init failed");
         }
         int value()
         {
            int ret;
            sem_getvalue(&semaphore, &ret);
            return ret;
         }
         bool Wait()
         {
            return sem_wait(&semaphore) == 0;
         }
         bool Wait(timespec const& duration)
         {
            return sem_timedwait(&semaphore, &duration) == 0;
         }
         bool Signal()
         {
            return sem_post(&semaphore) == 0;
         }
         ~Semaphore()
         {
            sem_destroy(&semaphore);
         }
      };
   } // namespace threads
} // namespace pc
