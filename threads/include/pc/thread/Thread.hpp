#pragma once

#include <pthread.h>

namespace pc
{
   namespace threads
   {
      class Thread
      {
         pthread_t    threadId;
         mutable bool cancellable;

       public:
         Thread(void* (*start_routine)(void*), void* arg = NULL) : cancellable(true)
         {
            pthread_create(&threadId, NULL, start_routine, arg);
         }
         Thread(Thread const& other) :
             threadId(other.threadId), cancellable(other.cancellable)
         {
            other.cancellable = false;
         }
         bool detach()
         {
            cancellable = pthread_detach(threadId) == 0;
            return cancellable;
         }
         bool join()
         {
            cancellable = (pthread_join(threadId, NULL) !=
                           0) /*Upon failure, thread can still be cancelled*/;
            return cancellable;
         }
         ~Thread()
         {
            if (!cancellable)
               return;
            pthread_cancel(threadId);
         }
      };
   } // namespace threads
} // namespace pc
