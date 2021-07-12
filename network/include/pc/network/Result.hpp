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
#ifdef PC_PROFILE
         timespec duration;
#endif

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
             NoOfBytes(0), SocketClosed(false), PollFailure(false),
             DeadlineFailure(false), Ignore(false)
#ifdef PC_PROFILE
             ,
             duration()
#endif
         {
         }
         Result& JoinInto(Result const& other)
         {
            SocketClosed    = SocketClosed || other.SocketClosed;
            PollFailure     = PollFailure || other.PollFailure;
            DeadlineFailure = DeadlineFailure || other.DeadlineFailure;
            Ignore          = Ignore || other.Ignore;
            return *this;
         }
      };
   } // namespace network
} // namespace pc
