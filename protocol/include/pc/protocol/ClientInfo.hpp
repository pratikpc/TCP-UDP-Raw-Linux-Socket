#pragma once

#include <pc/deadliner/Deadline.hpp>

#include <pc/protocol/types.hpp>

#include <string>

namespace pc
{
   namespace protocol
   {
      struct ClientInfo
      {
         int                     socket;
         std::string             clientId;
         pc::deadliner::Deadline deadline;
         ClientResponseCallback  callback;
         bool                    scheduleTermination;
         bool                    sendHeartbeat;

         static ClientInfo createClientInfo(int socket, ClientResponseCallback callback)
         {
            ClientInfo clientInfo;
            clientInfo.callback            = callback;
            clientInfo.socket              = socket;
            clientInfo.clientId            = "";
            clientInfo.scheduleTermination = false;
            clientInfo.sendHeartbeat       = false;
            return clientInfo;
         }

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
