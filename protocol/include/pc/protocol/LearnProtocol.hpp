#pragma once
#include <ctime>

namespace pc
{
   namespace protocol
   {
      struct LearnProtocol
      {
         std::time_t timeout;
         LearnProtocol(std::time_t timeout) : timeout(timeout) {}
      };
   } // namespace protocol
} // namespace pc
