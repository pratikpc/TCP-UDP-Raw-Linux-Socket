#include <cstdlib>
#include <iostream>
#include <sstream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>
#include <pc/protocol/ServerLearnProtocol.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

#include <sys/sysinfo.h>

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
   Protocol& poll = *((Protocol*)arg);
   while (true)
   {
      poll.Poll();
      poll.Execute();
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
   pc::network::TCP server(ip.bind());
   server.setReusable();
   server.listen();
   // server.keepAlive();
   ProtocolVec protocols(get_nprocs());

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
      std::cout << "\nTable Created";
   }

   for (ProtocolVec::iterator it = protocols.begin(); it != protocols.end(); ++it)
   {
      it->config        = &config;
      it->balancerIndex = (it - protocols.begin());
      it->timeout       = 10;
      pc::threads::Thread(&PollAndExecute, &(*it)).detach();
   }
   pc::threads::Thread(&execHealthCheck, &protocols).detach();

   while (true)
   {
      pc::network::TCP child(server.accept());
      if (child.invalid())
         continue;
      // child.keepAlive();
      std::size_t currentBalance = *balancer;
      protocols[currentBalance].Add(child.socket, pollCallback);
      std::cout << std::endl
                << "Connected to " << child.socket << " socket on " << currentBalance
                << " thread : Balancer" << balancer;
      child.invalidate();
   }
   return EXIT_SUCCESS;
}
