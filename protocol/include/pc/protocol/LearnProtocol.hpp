#pragma once

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/thread/Mutex.hpp>
#include <pc/thread/MutexGuard.hpp>

#include <pc/protocol/Config.hpp>

namespace pc
{
   namespace protocol
   {
      class LearnProtocol
      {
       public:
         Config*     config;
         std::size_t timeout;
      };
   } // namespace protocol
} // namespace pc
