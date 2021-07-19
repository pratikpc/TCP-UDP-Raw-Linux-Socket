#pragma once

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/network/TCP.hpp>

#include <pc/protocol/LearnProtocol.hpp>
#include <pc/protocol/Packet.hpp>
#include <pc/protocol/types.hpp>

#include <pc/deadliner/Deadline.hpp>

namespace pc
{
   namespace protocol
   {
      struct ClientLearnProtocol : public pc::protocol::LearnProtocol
      {
       private:
         network::TCP        server;
         deadliner::Deadline deadline;

       public:
         std::string const clientId;

         ClientLearnProtocol(network::TCP&      server,
                             std::string const& clientId,
                             std::time_t        timeout) :
             LearnProtocol(timeout),
             server(server), clientId(clientId)
         {
            assert(!this->server.invalid());
         }
         template <typename Buffer>
         NetworkPacket Read(Buffer& buffer)
         {
            // If deadline crossed, sleep
            if (deadline)
               return NetworkPacket(server.socket, Commands::DeadlineCrossed);
            NetworkPacket packet = NetworkPacket::Read(server, buffer, timeout);
            ++deadline;
            if (packet.command == Commands::HeartBeat)
            {
               NetworkPacket heartBeatReply(server.socket, Commands::Blank);
               Write(heartBeatReply);
               return Read(buffer);
            }
            return packet;
         }
         network::Result Write(NetworkPacket const& packet)
         {
            if (deadline)
            {
               network::Result result;
               result.DeadlineFailure = true;
               return result;
            }
            ++deadline;
            return packet.Write(server, timeout);
         }

         template <typename Buffer>
         network::Result SetupConnection(Buffer& buffer)
         {
            NetworkPacket const ackAck(server.socket, Commands::Setup::Ack);

            network::Result result = Write(ackAck);
            if (result.IsFailure())
               return result;
            NetworkPacket ackSyn = NetworkPacket::Read(server, buffer, timeout);
            if (ackSyn.command != Commands::Setup::Syn)
            {
               result.SocketClosed = true;
               return result;
            }
            NetworkPacket const clientIdSend(
                server.socket, Commands::Setup::ClientID, clientId);
            result = clientIdSend.Write(server, timeout);
            if (result.IsFailure())
               return result;
            NetworkPacket join = NetworkPacket::Read(server, buffer, timeout);
            if (join.command != Commands::Setup::Join)
            {
               result.SocketClosed = true;
               return result;
            }
            return result;
         }
      };
   } // namespace protocol
} // namespace pc
