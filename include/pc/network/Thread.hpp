#pragma once

#include <pthread.h>

namespace pc
{
   namespace network
   {
      class Thread
      {
         pthread_t threadId;

       public:
         Thread(void* (*start_routine)(void*), void* arg)
         {
            pthread_create(&threadId, NULL, start_routine, arg);
         }
         bool detach()
         {
            return pthread_detach(threadId) == 0;
         }
         ~Thread()
         {
            pthread_cancel(threadId);
         }
      };
   } // namespace network
} // namespace pc