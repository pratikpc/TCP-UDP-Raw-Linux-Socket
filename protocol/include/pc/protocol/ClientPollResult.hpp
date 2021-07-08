#pragma once

namespace pc
{
   namespace protocol
   {
      struct ClientPollResult
      {
         bool terminate;
         bool read;
         bool write;

         ClientPollResult() : terminate(false), read(false), write(false) {}
      };
   } // namespace protocol
} // namespace pc