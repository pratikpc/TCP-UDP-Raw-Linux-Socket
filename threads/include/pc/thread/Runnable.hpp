#pragma once

#include <pc/thread/Thread.hpp>

namespace pc
{
   namespace threads
   {
      class Runnable : public Thread
      {
         static void* InternalThreadEntryFunc(void* This)
         {
            ((Runnable*)This)->Run();
            return NULL;
         }

       public:
         Runnable() : Thread(Runnable::InternalThreadEntryFunc, this) {}

       protected:
         // Reimplement run
         virtual void Run() = 0;
      };
   } // namespace threads
} // namespace pc
