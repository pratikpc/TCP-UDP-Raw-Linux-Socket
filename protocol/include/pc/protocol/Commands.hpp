#pragma once

#include <string>

namespace pc
{
   namespace protocol
   {
      namespace commands
      {
         std::string const Send = "SEND";
         std::string const Empty = "EMTY";
         namespace setup
         {
            std::string const Ack        = "ACKA";
            std::string const Syn        = "ACKS";
            std::string const ClientID = "ACID";
            std::string const Join      = "JOIN";
         } // namespace setup
         namespace downdetect
         {
            std::string const DownCheck = "DWNC";
            std::string const DownAlive = "DWNA";
         } // namespace downdetect
      }    // namespace commands
   }       // namespace protocol
} // namespace pc
