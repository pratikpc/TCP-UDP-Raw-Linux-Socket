#pragma once

#include <pc/deadliner/Deadline.hpp>

#include <pc/protocol/types.hpp>

#include <string>

namespace pc
{
   namespace protocol
   {
      class ClientInfo
      {
       public:
         std::string             clientId;
         pc::deadliner::Deadline deadline;
         ClientResponseCallback  callback;

         bool scheduleTermination;

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
