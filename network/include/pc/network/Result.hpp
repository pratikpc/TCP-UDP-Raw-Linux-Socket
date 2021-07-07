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

         bool IsFailure() const
         {
            return NoOfBytes == 0 || SocketClosed || PollFailure || DeadlineFailure;
         }
         bool isSuccess() const
         {
            return !IsFailure();
         }

         Result() :
             NoOfBytes(0), SocketClosed(false), PollFailure(false), DeadlineFailure(false)
         {
         }
      };
   } // namespace network
} // namespace pc
