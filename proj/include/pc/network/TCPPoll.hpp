#pragma once

#include <pc/dataQueue.hpp>
#include <poll.h>

namespace pc
{
   namespace network
   {
      class TCPPoll
      {
       public:
         typedef pc::DataQueue<pollfd> PollFdQueue;
         PollFdQueue                   dataQueue;

       public:
         TCPPoll& operator+=(pollfd const& val)
         {
            dataQueue += val;
            return *this;
         }
         TCPPoll& operator-=(std::size_t index)
         {
            dataQueue -= index;
            return *this;
         }

         int poll(std::size_t timeout)
         {
            if (size() == 0)
            {
               sleep(timeout / 1000);
               return 0; // Timeout
            }
            int const rv = ::poll(dataQueue.data(), size(), timeout);
            if (rv == -1)
               throw std::runtime_error("Poll failed");
            return rv;
         }

         std::size_t size() const
         {
            return dataQueue.size();
         }

         void PerformUpdate()
         {
            return dataQueue.PerformUpdate();
         }
      };
   } // namespace network
} // namespace pc