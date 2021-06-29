#pragma once

#include <pthread.h>

namespace pc
{
   namespace network
   {
      class Thread
      {
         pthread_t threadId;
         bool      detached;

       public:
         Thread(void* (*start_routine)(void*), void* arg) : detached(false)
         {
            pthread_create(&threadId, NULL, start_routine, arg);
         }
         bool detach()
         {
            detached = true;
            return pthread_detach(threadId) == 0;
         }
         ~Thread()
         {
            if (!detached)
               pthread_cancel(threadId);
         }
      };
   } // namespace network
} // namespace pc