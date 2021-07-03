#pragma once

#include <pc/deadliner/deadline.hpp>
#include <string>

namespace pc
{
   namespace protocol
   {
      class ClientInfo
      {
       public:
         typedef void (*Callback)(pollfd const&, ClientInfo&);

         std::string  clientId;
         pc::Deadline deadline;
         Callback     callback;

         int socket;

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
   } // namespace protocol
} // namespace pc