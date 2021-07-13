#pragma once

namespace pc
{
   namespace threads
   {
      template <typename Mutex>
      class GenericMutexGuard
      {
         Mutex& mutex;

       public:
         GenericMutexGuard(Mutex& mutex) : mutex(mutex)
         {
            mutex.Lock();
         }
         ~GenericMutexGuard()
         {
            mutex.Unlock();
         }
      };
   } // namespace threads
} // namespace pc
