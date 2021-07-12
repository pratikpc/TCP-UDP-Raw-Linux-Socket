#pragma once

#include <pthread.h>
#include <sys/sysinfo.h>

namespace pc
{
   namespace threads
   {
      int ProcessorCount()
      {
         return get_nprocs();
      }
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
            cancellable = false;
            return pthread_detach(threadId) == 0;
         }
         bool join()
         {
            cancellable = (pthread_join(threadId, NULL) !=
                           0) /*Upon failure, thread can still be cancelled*/;
            return cancellable;
         }
         ~Thread()
         {
            if (cancellable)
               pthread_cancel(threadId);
         }
      };
   } // namespace threads
} // namespace pc
