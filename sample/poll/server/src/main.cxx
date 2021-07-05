#include <cstdlib>
#include <iostream>
#include <sstream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>
#include <pc/protocol/ServerLearnProtocol.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

#include <sys/sysinfo.h>

typedef pc::protocol::ServerLearnProtocol Protocol;
typedef std::vector<Protocol>             ServerLearnProtocols;

std::string repeat(std::string value, std::size_t times)
{
   std::ostringstream stream;
   for (std::size_t i = 0; i < times; ++i)
      stream << value;
   return stream.str();
}

pc::protocol::NetworkSendPacket pollCallback(pc::protocol::NetworkPacket const& packet,
                                             pc::protocol::ClientInfo const& clientInfo)
{
   std::string const response = packet.data + " was received from " + clientInfo.clientId;
   pc::protocol::NetworkSendPacket responsePacket(response);
   return responsePacket;
}

void* execTcp(void* arg)
{
   Protocol& poll = *((Protocol*)arg);
   while (true)
   {
      poll.poll();
      poll.execHealthCheck();
   }
   return NULL;
}

void downCallback(std::size_t const idx)
{
   std::cout << "One client went down at " << idx << " balancer" << std::endl;
}

int main()
{
   pc::network::IP ip(SOCK_STREAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::TCP tcp(ip.bind());
   tcp.setReusable();
   tcp.listen();
   ServerLearnProtocols protocols(get_nprocs());

   pc::balancer::priority balancer(protocols.size());

   pc::protocol::Config config("postgresql://postgres@localhost:5432/");
   config.downCallback = &downCallback;
   config.balancer     = &balancer;

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

   for (ServerLearnProtocols::iterator it = protocols.begin(); it != protocols.end();
        ++it)
   {
      it->config        = &config;
      it->balancerIndex = (it - protocols.begin());
      it->timeout       = 10;
      pc::threads::Thread(&execTcp, &(*it)).detach();
   }

   while (true)
   {
      std::cout << std::endl << "Try to connect";
      pc::network::TCP child(tcp.accept());
      if (child.invalid())
         continue;
      child.keepAlive();
      std::size_t currentBalance = *balancer;
      protocols[currentBalance].Add(child.socket, pollCallback);
      std::cout << std::endl
                << "Connected to " << child.socket << " on " << currentBalance
                << " thread : Balancer" << balancer;
      child.invalidate();
   }
   return EXIT_SUCCESS;
}
