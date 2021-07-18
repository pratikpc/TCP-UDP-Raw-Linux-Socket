#pragma once

#include <list>
#include <pc/protocol/Packet.hpp>

namespace pc
{
   namespace protocol
   {
      class ClientInfo;
      typedef RawPacket<2>             NetworkPacket;
      typedef RawSendPacket<2>         NetworkSendPacket;
      typedef std::list<NetworkPacket> PacketList;
      typedef void(ClientResponseCallback)(NetworkPacket&, ClientInfo const&);
   } // namespace protocol
} // namespace pc
