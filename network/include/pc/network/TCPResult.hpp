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

         bool IsFailure()
         {
            return NoOfBytes == 0 || SocketClosed || PollFailure;
         }

         TCPResult() : NoOfBytes(0), SocketClosed(false), PollFailure(false) {}
      };
   } // namespace network
} // namespace pc