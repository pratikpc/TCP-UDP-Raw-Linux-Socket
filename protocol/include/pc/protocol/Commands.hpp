#pragma once

#include <string>

namespace pc
{
   namespace protocol
   {
      namespace Commands
      {
         std::string const Send  = "SEND";
         std::string const Empty = "EMTY";
         namespace Setup
         {
            std::string const Ack      = "ACKA";
            std::string const Syn      = "ACKS";
            std::string const ClientID = "ACID";
            std::string const Join     = "JOIN";
         } // namespace Setup
         namespace DownDetect
         {
            std::string const DownCheck = "DWNC";
            std::string const DownAlive = "DWNA";
         } // namespace DownDetect
      } // namespace Commands
   }    // namespace protocol
} // namespace pc
