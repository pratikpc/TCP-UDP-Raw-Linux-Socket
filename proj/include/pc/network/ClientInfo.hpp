#pragma once

#include <pc/deadline.hpp>
#include <string>

namespace pc
{
   namespace network
   {
      class ClientInfo
      {
       public:
         typedef void (*Callback)(pollfd const&,
                                  ClientInfo&,
                                  void* /*Configuration details*/);

         std::string  clientId;
         pc::Deadline deadline;
         Callback     callback;

         bool deadlineBreach() const
         {
            return deadline;
         }

         bool hasClientId() const
         {
            return !clientId.empty();
         }

         ClientInfo& changeMaxCount(std::size_t maxCount)
         {
            deadline.MaxCount(maxCount);
            return *this;
         }
      };
   } // namespace network
} // namespace pc