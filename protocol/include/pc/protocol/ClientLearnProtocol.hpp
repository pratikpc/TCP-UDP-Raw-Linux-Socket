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
            // if (clientInfos[server.fd].deadline)
            // {
            //    sleep(timeout / 1000);
            //    return NetworkPacket(commands::Empty);
            // }
            NetworkPacket packet = NetworkPacket::Read(server, buffer, timeout);
            if (packet.command == commands::downdetect::DownCheck)
            {
               NetworkPacket alive(commands::downdetect::DownAlive);
               alive.Write(server, timeout);
               return Read(buffer);
            }
            return packet;
         }
         void Send(NetworkSendPacket const& packet)
         {
            return packet.Write(server, timeout);
         }
         void SetupConnection()
         {
            NetworkPacket const ackAck(commands::setup::Ack);
            ackAck.Write(server, timeout);

            network::buffer data(40);
            NetworkPacket   ackSyn = NetworkPacket::Read(server, data, timeout);
            if (ackSyn.command != commands::setup::Syn)
            {
               throw std::runtime_error(commands::setup::Syn +
                                        " not received. Protocol violated");
            }
            NetworkPacket const clientIdSend(commands::setup::ClientID, clientId);
            clientIdSend.Write(server, timeout);
            NetworkPacket join = NetworkPacket::Read(server, data, timeout);
            if (join.command != commands::setup::Join)
            {
               throw std::runtime_error(commands::setup::Join +
                                        " not received. Protocol violated");
            }
         }
      };
   } // namespace protocol
} // namespace pc
