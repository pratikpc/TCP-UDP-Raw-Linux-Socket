#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <pc/memory/Buffer.hpp>
#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>
#include <pc/protocol/ServerLearnProtocol.hpp>

#include <pc/thread/Thread.hpp>

#include <pc/balancer/priority.hpp>

namespace protocol = pc::protocol;
typedef protocol::ServerLearnProtocol Protocol;
typedef std::vector<Protocol>         ProtocolVec;

typedef std::tr1::unordered_set<int /*socket*/> UniqueSockets;

std::string repeat(std::string value, std::size_t times)
{
   std::ostringstream stream;
   for (std::size_t i = 0; i < times; ++i)
      stream << value;
   return stream.str();
}

void pollCallback(protocol::NetworkPacket& packet, protocol::ClientInfo const& clientInfo)
{
   std::string const response = packet.data + " was received from " + clientInfo.clientId;
   packet.data                = response;
}

void* PollAndRead(void* arg)
{
   using namespace pc::memory;
   Protocol&    poll = *((Protocol*)arg);
   Buffer<char> buffer(106 * 1);
#ifdef PC_NETWORK_MOCK
   pc::protocol::NetworkPacket packet(
       1 /*Socket unused*/, pc::protocol::Commands::Send, repeat("JK", 10));
   std::string const& packetData = packet.Marshall();
   std::copy(packetData.begin(), packetData.end(), buffer.begin());
#endif
   while (true)
   {
      if (poll.size() == 0)
      {
         sleep(poll.timeout);
         continue;
      }
      poll.Poll<UniqueSockets>(buffer);
#ifndef PC_SEPARATE_POLL_EXEC_WRITE
      poll.Execute(pollCallback);
      poll.Write();
#endif
   }
   return NULL;
}

#ifdef PC_SEPARATE_POLL_EXEC_WRITE
void* ExecuteAndWrite(void* arg)
{
   Protocol& poll = *((Protocol*)arg);
   while (true)
   {
      if (poll.size() == 0)
      {
         sleep(poll.timeout);
         continue;
      }
      poll.Execute(pollCallback);
      poll.Write();
   }
   return NULL;
}
#endif

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

#ifdef PC_PROFILE
   std::cout << "Profiling mode on" << std::endl;
#endif
#ifdef PC_NETWORK_MOCK
   std::cout << "Network mock on" << std::endl;
#endif
#ifdef PC_USE_SPINLOCKS
   std::cout << "Spinlock mode on" << std::endl;
#endif
#ifndef PC_DISABLE_DATABASE_SUPPORT
   std::cout << "Database support on" << std::endl;
#endif
#ifdef PC_OPTIMIZE_READ_MULTIPLE_SINGLE_SHOT
   std::cout << "Optimize reads by reading multiple elements" << std::endl;
#endif
#ifdef PC_SEPARATE_POLL_EXEC_WRITE
   std::cout << "Separate poll, exec and write" << std::endl;
#endif
   pc::balancer::priority balancer(/*pc::threads::ProcessorCount()*/ 1);
   protocol::Config       config(
       "postgresql://postgres@localhost:5432/", balancer, downCallback);
   ProtocolVec protocols(balancer.MaxCount(), config);

#ifndef PC_DISABLE_DATABASE_SUPPORT
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
#endif

   for (ProtocolVec::iterator it = protocols.begin(); it != protocols.end(); ++it)
   {
      it->balancerIndex = (it - protocols.begin());
      pc::threads::Thread thread1(&PollAndRead, &(*it));
      thread1.StickToCore(2);
      thread1.detach();
#ifdef PC_SEPARATE_POLL_EXEC_WRITE
      pc::threads::Thread thread2(&ExecuteAndWrite, &(*it));
      thread2.detach();
      thread2.StickToCore(3);
#endif
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
      protocols[currentBalance].Add(child.socket);
      std::cout << "Connected to " << child.socket << " socket on " << currentBalance
                << " thread : Balancer" << balancer << std::endl;
      child.invalidate();
   }
   healthCheckThread.join();
   return EXIT_SUCCESS;
}
