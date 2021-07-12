#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>
#include <pc/network/types.hpp>
#include <pc/protocol/ServerLearnProtocol.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

namespace protocol = pc::protocol;
typedef protocol::ServerLearnProtocol Protocol;
typedef std::vector<Protocol>         ProtocolVec;

std::string repeat(std::string value, std::size_t times)
{
   std::ostringstream stream;
   for (std::size_t i = 0; i < times; ++i)
      stream << value;
   return stream.str();
}

protocol::NetworkSendPacket pollCallback(protocol::NetworkPacket const& packet,
                                         protocol::ClientInfo const&    clientInfo)
{
   std::string const response = packet.data + " was received from " + clientInfo.clientId;
   protocol::NetworkSendPacket responsePacket(response);
   return responsePacket;
}

void* PollAndExecute(void* arg)
{
   using namespace pc::network;
   Protocol& poll = *((Protocol*)arg);
   buffer    buffer(UINT16_MAX);
   while (true)
   {
      poll.Poll(buffer);
      poll.Execute();
      poll.Write();
   }
   return NULL;
}

void* execHealthCheck(void* arg)
{
   ProtocolVec& protocols = *((ProtocolVec*)(arg));
   while (true)
   {
      sleep(10);
      for (ProtocolVec::iterator it = protocols.begin(); it != protocols.end(); ++it)
         it->execHealthCheck();
   }
}

void downCallback(std::size_t const idx, std::size_t count)
{
   std::cout << count << " client went down at " << idx << " balancer" << std::endl;
}

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr << std::endl;
   std::cout << "Hostname = " << ip.hostName() << std::endl;
   pc::network::TCP server(ip.bind());
   server.setReusable();
   // server.keepAlive();
   server.speedUp();
   server.listen();

   ProtocolVec protocols(/*pc::threads::ProcessorCount()*/ 1);

   pc::balancer::priority balancer(protocols.size());

   protocol::Config config(
       "postgresql://postgres@localhost:5432/", balancer, &downCallback);

   {
      pc::pqpp::Result res = config.connection.exec(
          "CREATE TABLE IF NOT EXISTS priority_table ("
          "clientId varchar(45) NOT NULL,"
          "priority smallint NOT NULL,"
          "PRIMARY KEY(clientId)) ");
      if (!res)
      {
         std::cerr << "Failed to create table\n";
         return EXIT_FAILURE;
      }
      std::cout << "Table Created\n";
   }

   for (ProtocolVec::iterator it = protocols.begin(); it != protocols.end(); ++it)
   {
      it->config        = &config;
      it->balancerIndex = (it - protocols.begin());
      it->timeout       = 10;
      pc::threads::Thread thread(&PollAndExecute, &(*it));
      thread.StickToCore(2);
      thread.detach();
   }
   pc::threads::Thread healthCheckThread(&execHealthCheck, &protocols);
   healthCheckThread.StickToCore(1);
   while (true)
   {
      std::cout << "Try to connect" << std::endl;
      pc::network::TCP child(server.accept());
      if (child.invalid())
         continue;
      // child.keepAlive();
      child.speedUp();
      std::size_t currentBalance = *balancer;
      protocols[currentBalance].Add(child.socket, pollCallback);
      std::cout << "Connected to " << child.socket << " socket on " << currentBalance
                << " thread : Balancer" << balancer << std::endl;
      child.invalidate();
   }
   healthCheckThread.join();
   return EXIT_SUCCESS;
}
