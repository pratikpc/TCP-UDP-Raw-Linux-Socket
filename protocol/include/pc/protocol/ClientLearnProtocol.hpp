#pragma once

#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include <sys/ioctl.h>
#include <unistd.h>

#include <pc/network/TCP.hpp>
#include <pc/network/TCPPoll.hpp>

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

         ClientLearnProtocol(network::TCP& server, std::string const& clientId) :
             server(server), clientId(clientId)
         {
            assert(!this->server.invalid());
         }
         NetworkPacket Read(network::buffer& buffer)
         {
            // If deadline crossed, sleep
            if (deadline)
               return NetworkPacket(Commands::DeadlineCrossed);
            NetworkPacket packet = NetworkPacket::Read(server, buffer, timeout);
            ++deadline;
            if (packet.command == Commands::HeartBeat)
            {
               NetworkPacket heartBeatReply(Commands::Blank);
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
         network::Result SetupConnection()
         {
            NetworkPacket const ackAck(Commands::Setup::Ack);

            network::Result result = Write(ackAck);
            if (result.IsFailure())
               return result;
            network::buffer data(40);
            NetworkPacket   ackSyn = NetworkPacket::Read(server, data, timeout);
            if (ackSyn.command != Commands::Setup::Syn)
            {
               std::cerr << Commands::Setup::Syn << " not received. Protocol violated";
               result.SocketClosed = true;
               return result;
            }
            NetworkPacket const clientIdSend(Commands::Setup::ClientID, clientId);
            result = clientIdSend.Write(server, timeout);
            if (result.IsFailure())
               return result;
            NetworkPacket join = NetworkPacket::Read(server, data, timeout);
            if (join.command != Commands::Setup::Join)
            {
               result.SocketClosed = true;
               std::cerr << Commands::Setup::Join << " not received. Protocol violated";
               return result;
            }
            return result;
         }
      };
   } // namespace protocol
} // namespace pc
