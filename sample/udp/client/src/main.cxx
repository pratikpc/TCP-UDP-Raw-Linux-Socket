#include <iostream>

#include <pc/network/UDP.hpp>
#include <pc/network/ip.hpp>

#include <thread>

int main()
{
   pc::network::IP ip(SOCK_DGRAM);
   ip.load("127.0.0.1", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\nHostname = " << pc::network::IP::hostName();
   pc::network::UDP udp{ip.connect()};
   while (true)
   {
      std::string message;
         std::cout << "Send message\n";
      if (std::getline(std::cin, message))
      {
         udp.send((const void*)message.data(), message.size(), ip);
      }
   }
   return EXIT_SUCCESS;
}
