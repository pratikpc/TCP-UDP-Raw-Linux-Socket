#include <iostream>

#include <pc/network/UDP.hpp>
#include <pc/network/ip.hpp>

#include <cstdlib>

int main()
{
   pc::network::IP ip(SOCK_DGRAM);
   ip.hints.ai_flags = AI_PASSIVE; // Use current IP
   ip.load("", "9900");

   std::string ipstr = ip;
   std::cout << "IP = " << ipstr;
   std::cout << "\n Hostname = " << ip.hostName();
   pc::network::UDP udp(ip.bind());

   while (1)
   {
      std::vector<char> recv = udp.recv(1000);
      if (recv.empty())
         break;
      recv[1000 - 1] = '\0';
      std::cout << "Client said : " << recv.data() << "\n";
   }
   return EXIT_SUCCESS;
}
