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
               return NetworkPacket(Commands::Empty);
            NetworkPacket packet = NetworkPacket::Read(server, buffer, timeout);
            ++deadline;
            return packet;
         }
         network::TCPResult Write(NetworkSendPacket const& packet) const
         {
            return packet.Write(server, timeout);
         }
         void SetupConnection() const
         {
            NetworkPacket const ackAck(Commands::Setup::Ack);
            ackAck.Write(server, timeout);

            network::buffer data(40);
            NetworkPacket   ackSyn = NetworkPacket::Read(server, data, timeout);
            if (ackSyn.command != Commands::Setup::Syn)
            {
               throw std::runtime_error(Commands::Setup::Syn +
                                        " not received. Protocol violated");
            }
            NetworkPacket const clientIdSend(Commands::Setup::ClientID, clientId);
            clientIdSend.Write(server, timeout);
            NetworkPacket join = NetworkPacket::Read(server, data, timeout);
            if (join.command != Commands::Setup::Join)
            {
               throw std::runtime_error(Commands::Setup::Join +
                                        " not received. Protocol violated");
            }
         }
      };
   } // namespace protocol
} // namespace pc
