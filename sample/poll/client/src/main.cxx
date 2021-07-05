#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <pc/protocol/ClientLearnProtocol.hpp>

#include <cstdlib>
#include <ctime>
#include <sstream>

#include <pc/lexical_cast.hpp>

void* func(void* clientIndexPtr)
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::string      ipstr = ip;
   pc::network::TCP tcp(ip.connect());
   std::cout << "\nHostname = " << pc::network::IP::hostName();
   tcp.keepAlive();

   std::cout << std::endl << "IP = " << ipstr;

   int clientIndex = *((int*)clientIndexPtr);

   std::string const clientId = "CLIENT-" + pc::lexical_cast(clientIndex);
   std::cout << std::endl << "ClientId = " << clientId;

   pc::protocol::ClientLearnProtocol protocol(tcp.socket, clientId);
   protocol.timeout = 10;
   protocol.setupConnection();

   pc::network::buffer buffer(200);
   pollfd              server = protocol.getServer();
   for (std::size_t i = 0; true; i++)
   {
      std::cout << std::endl << "Message sending " << i << " at " << protocol.clientId;
      pc::protocol::NetworkSendPacket packet("Hi server from " + protocol.clientId);
      packet.Write(server, protocol.timeout);
      pc::protocol::NetworkPacket responsePacket = protocol.clientExecCallback(buffer);
      if (responsePacket.command != pc::protocol::commands::Empty)
         std::cout << "Server says: " << responsePacket.data.size() << " : "
                   << responsePacket.data << " at " << protocol.clientId << std::endl;
      else
         std::cout << std::endl << "No communication at " << protocol.clientId;
      usleep(2 * 1000 * 1000 / 25);
      // sleep(32);
   }
   return NULL;
}

#include <pc/thread/Thread.hpp>
int main()
{
   std::vector<int> count(10);
   for (std::size_t i = 0; i < count.size(); ++i)
   {
      count[i] = i;
      pc::threads::Thread(func, &count[i]).detach();
   }
   std::cout << "Hi\n";
   for (std::size_t i = 0; i < 100; ++i)
   {
      sleep(UINT32_MAX);
   }
}
