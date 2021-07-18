#pragma once

#include <pc/protocol/Packet.hpp>

namespace pc
{
   namespace protocol
   {
      class ClientInfo;
      typedef RawPacket<2>             NetworkPacket;
      typedef RawSendPacket<2>         NetworkSendPacket;
      typedef void(ClientResponseCallback)(NetworkPacket&, ClientInfo const&);
   } // namespace protocol
} // namespace pc
