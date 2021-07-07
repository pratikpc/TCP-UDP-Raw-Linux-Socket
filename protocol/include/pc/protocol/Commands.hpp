#pragma once

#include <string>

namespace pc
{
   namespace protocol
   {
      namespace Commands
      {
         std::string const Send            = "SEND";
         std::string const DeadlineCrossed = "DEDC";
         std::string const Empty           = "EMTY";
         std::string const HeartBeat       = "HART";
         std::string const Blank           = "BLNK";
         namespace Setup
         {
            std::string const Ack      = "ACKA";
            std::string const Syn      = "ACKS";
            std::string const ClientID = "ACID";
            std::string const Join     = "JOIN";
         } // namespace Setup
         namespace MajorErrors
         {
            std::string const SocketClosed = "ERCL";
         }
      } // namespace Commands
   }    // namespace protocol
} // namespace pc
