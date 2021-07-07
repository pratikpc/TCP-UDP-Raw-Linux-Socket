#pragma once

#include <cstddef>

namespace pc
{
   namespace network
   {
      struct Result
      {
         std::size_t NoOfBytes;

         bool SocketClosed;
         bool PollFailure;
         bool DeadlineFailure;
         bool Ignore;

         bool IsFailure() const
         {
            // Receiving ignore is not a sign of failure
            return NoOfBytes == 0 || SocketClosed || PollFailure || DeadlineFailure;
         }
         bool IsSuccess() const
         {
            return !IsFailure();
         }

         Result() :
             NoOfBytes(0), SocketClosed(false), PollFailure(false), DeadlineFailure(false), Ignore(false)
         {
         }
      };
   } // namespace network
} // namespace pc
