#include <iostream>

#include <pc/network/TCP.hpp>
#include <pc/network/ip.hpp>

#include <pc/protocol/ClientLearnProtocol.hpp>

#include <cstdlib>
#include <ctime>
#include <sstream>

#include <pc/lexical_cast.hpp>

std::string repeat(std::string value, std::size_t times)
{
   std::ostringstream stream;
   for (std::size_t i = 0; i < times; ++i)
      stream << value;
   return stream.str();
}

void* func(void* clientIndexPtr)
{
   pc::network::IP ip(SOCK_STREAM);
   ip.load("127.0.0.1", "9900");

   std::cout << "\nHostname = " << pc::network::IP::hostName();
   std::string ipstr = ip;
   std::cout << std::endl << "IP = " << ipstr;
   pc::network::TCP server(ip.connect());
   server.keepAlive();

   int const clientIndex = *((int*)clientIndexPtr);

   std::string const clientId = "CLIENT-" + pc::lexical_cast(clientIndex);
   std::cout << std::endl << "ClientId = " << clientId;

   pc::protocol::ClientLearnProtocol protocol(server, clientId);
   protocol.timeout = 10;
   protocol.SetupConnection();

   pc::network::buffer             buffer(200);
   pc::protocol::NetworkSendPacket packet(
       repeat("Hi server from " + protocol.clientId, 10));
   for (std::size_t i = 0; true; i++)
   {
      std::cout << std::endl << "Message sending " << i << " at " << protocol.clientId;
      pc::protocol::NetworkSendPacket packet("Hi server from " + protocol.clientId);

      pc::network::TCPResult result = protocol.Write(packet);
      if (result.IsFailure())
         break;
      pc::protocol::NetworkPacket responsePacket = protocol.Read(buffer);
      if (responsePacket.command == pc::protocol::Commands::MajorErrors::SocketClosed)
         break;
      else if (responsePacket.command != pc::protocol::Commands::Empty)
         std::cout << "Server says: " << responsePacket.data.size() << " : "
                   << responsePacket.data << " at " << protocol.clientId << std::endl;
      else
         std::cout << std::endl << "No communication at " << protocol.clientId;
      usleep(2 * 1000 * 1000 / 25);
      // sleep(32);
   }
   std::cout << "Iteration over" << std::endl;
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
