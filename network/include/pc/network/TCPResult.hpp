#pragma once

#include <cstddef>

namespace pc
{
   namespace network
   {
      struct TCPResult
      {
         std::size_t NoOfBytes;

         bool SocketClosed;
         bool PollFailure;
         bool DeadlineFailure;

         bool IsFailure() const
         {
            return NoOfBytes == 0 || SocketClosed || PollFailure || DeadlineFailure;
         }

         TCPResult() :
             NoOfBytes(0), SocketClosed(false), PollFailure(false), DeadlineFailure(false)
         {
         }
      };
   } // namespace network
} // namespace pc
